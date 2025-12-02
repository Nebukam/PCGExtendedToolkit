// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExMT.h"
#include "Tasks/Task.h"
#include "PCGExHelpers.h"
#include "PCGExContext.h"
#include "PCGExGlobalSettings.h"
#include "PCGExSubSystem.h"
#include "HAL/PlatformTime.h"

#define PCGEX_TASK_LOG(...) UE_LOG(__VA_ARGS__)
#define PCGEX_MULTI_LOG(...) UE_LOG(__VA_ARGS__)
#define PCGEX_MANAGER_LOG(...) UE_LOG(__VA_ARGS__)

namespace PCGExMT
{
	void SetWorkPriority(const EPCGExAsyncPriority Selection, UE::Tasks::ETaskPriority& Priority)
	{
		switch (Selection)
		{
		default:
		case EPCGExAsyncPriority::Default:
			SetWorkPriority(GetDefault<UPCGExGlobalSettings>()->GetDefaultWorkPriority(), Priority);
			break;
		case EPCGExAsyncPriority::Normal: Priority = UE::Tasks::ETaskPriority::Normal;
			break;
		case EPCGExAsyncPriority::High: Priority = UE::Tasks::ETaskPriority::High;
			break;
		case EPCGExAsyncPriority::BackgroundHigh: Priority = UE::Tasks::ETaskPriority::BackgroundHigh;
			break;
		case EPCGExAsyncPriority::BackgroundNormal: Priority = UE::Tasks::ETaskPriority::BackgroundNormal;
			break;
		case EPCGExAsyncPriority::BackgroundLow: Priority = UE::Tasks::ETaskPriority::BackgroundLow;
			break;
		}
	}

	FScope::FScope(const int32 InStart, const int32 InCount, const int32 InLoopIndex)
		: Start(InStart), Count(InCount), End(InStart + InCount), LoopIndex(InLoopIndex)
	{
	}

	void FScope::GetIndices(TArray<int32>& OutIndices) const
	{
		OutIndices.SetNumUninitialized(Count);
		for (int i = 0; i < Count; i++) { OutIndices[i] = Start + i; }
	}

	int32 SubLoopScopes(TArray<FScope>& OutSubRanges, const int32 MaxItems, const int32 RangeSize)
	{
		OutSubRanges.Empty();
		OutSubRanges.Reserve((MaxItems + RangeSize - 1) / RangeSize);
		for (int32 Idx = 0; Idx < MaxItems; Idx += RangeSize)
		{
			OutSubRanges.Emplace(Idx, FMath::Min(RangeSize, MaxItems - Idx), OutSubRanges.Num());
		}
		return OutSubRanges.Num();
	}

	void AssertEmptyThread(const int32 MaxItems)
	{
		PCGEX_TASK_LOG(LogPCGEx, Error, TEXT("StartRanges: MaxItems = %i - Graph will never finish! Enable bAssertOnEmptyThread for stack trace."), MaxItems);
		if (GetDefault<UPCGExGlobalSettings>()->bAssertOnEmptyThread) { ensure(false); }
	}

	// IAsyncHandle
	IAsyncHandle::~IAsyncHandle()
	{
		if (GetState() != EAsyncHandleState::Ended)
		{
			Cancel();
			Complete();
		}
	}

	bool IAsyncHandle::SetRoot(const TSharedPtr<IAsyncMultiHandle>& InRoot, const int32 InHandleIdx)
	{
		if (!InRoot) { return false; }
		HandleIdx = InHandleIdx;
		Root = InRoot;
		PCGEX_TASK_LOG(LogTemp, Warning, TEXT("IAsyncHandle[#%d|%s]::SetRoot to [#%d|%s]"), HandleIdx, *DEBUG_HandleId(), InRoot->HandleIdx, *InRoot->DEBUG_HandleId());
		return true;
	}

	void IAsyncHandle::SetParent(const TSharedPtr<IAsyncMultiHandle>& InParent)
	{
		if (!InParent) { return; }
		ParentHandle = InParent;
		PCGEX_TASK_LOG(LogTemp, Warning, TEXT("IAsyncHandle[#%d|%s]::SetParent to [#%d|%s]"), HandleIdx, *DEBUG_HandleId(), InParent->HandleIdx, *InParent->DEBUG_HandleId());

		// Only notify parent if the task isn't expected already.
		// This greatly facilitates batching and avoid race conditions where starting a task inside a loop
		// triggers completion before the next iteration can even be scheduled
		if (!bExpected)
		{
			bExpected = true;
			InParent->RegisterExpected();
		}
	}

	bool IAsyncHandle::Start()
	{
		if (!TryTransitionState(EAsyncHandleState::Idle, EAsyncHandleState::Running))
		{
			PCGEX_TASK_LOG(LogTemp, Error, TEXT("IAsyncHandle[#%d|%s]::Start (aborted)"), HandleIdx, *DEBUG_HandleId());
			Cancel();
			Complete();
			return false;
		}

		PCGEX_TASK_LOG(LogTemp, Warning, TEXT("IAsyncHandle[#%d|%s]::Start"), HandleIdx, *DEBUG_HandleId());
		if (const TSharedPtr<IAsyncMultiHandle> Parent = ParentHandle.Pin()) { Parent->NotifyStarted(); }
		//if (const TSharedPtr<IAsyncMultiHandle> RootPtr = Root.Pin()) { RootPtr->NotifyStarted(); }
		return true;
	}

	void IAsyncHandle::Cancel()
	{
		bool Expected = false;
		if (!bCancelled.compare_exchange_strong(Expected, true, std::memory_order_acq_rel)) { return; }

		// Try to end immediately if idle
		if (TryTransitionState(EAsyncHandleState::Idle, EAsyncHandleState::Ended))
		{
			PCGEX_TASK_LOG(LogTemp, Warning, TEXT("IAsyncHandle[#%d|%s]::Cancel"), HandleIdx, *DEBUG_HandleId());
			OnEnd(true);
		}
		// else { PCGEX_LOG(LogTemp, Error, TEXT("IAsyncHandle[#%d|%s]::Cancel (aborted)"), HandleIdx, *DEBUG_HandleId()); }
	}

	void IAsyncHandle::Complete()
	{
		if (TryTransitionState(EAsyncHandleState::Running, EAsyncHandleState::Ended))
		{
			PCGEX_TASK_LOG(LogTemp, Warning, TEXT("IAsyncHandle[#%d|%s]::Complete"), HandleIdx, *DEBUG_HandleId());
			OnEnd(IsCancelled());
		}
		//else { PCGEX_LOG(LogTemp, Error, TEXT("IAsyncHandle[#%d|%s]::Complete (aborted)"), HandleIdx, *DEBUG_HandleId()); }
	}

	bool IAsyncHandle::TryTransitionState(EAsyncHandleState From, EAsyncHandleState To)
	{
		return State.compare_exchange_strong(From, To, std::memory_order_acq_rel);
	}

	void IAsyncHandle::OnEnd(const bool bWasCancelled)
	{
		PCGEX_TASK_LOG(LogTemp, Warning, TEXT("IAsyncHandle[#%d|%s]::OnEnd"), HandleIdx, *DEBUG_HandleId());
		if (const TSharedPtr<IAsyncMultiHandle> Parent = ParentHandle.Pin())
		{
			// Notify parent of completion
			Parent->NotifyCompleted();
		}
	}

	// IAsyncMultiHandle
	IAsyncMultiHandle::IAsyncMultiHandle(const FName InName) : GroupName(InName)
	{
	}

	IAsyncMultiHandle::~IAsyncMultiHandle()
	{
		if (GetState() != EAsyncHandleState::Ended)
		{
			Cancel();
			Complete();
		}
	}

	bool IAsyncMultiHandle::RegisterExpected(int32 Count)
	{
		if (!IsAvailable()) { return false; }
		ExpectedCount.fetch_add(Count, std::memory_order_acq_rel);
		PCGEX_MULTI_LOG(LogTemp, Warning, TEXT("IAsyncMultiHandle[#%d|%s]::RegisterExpected = %d"), HandleIdx, *DEBUG_HandleId(), ExpectedCount.load());
		return true;
	}

	void IAsyncMultiHandle::NotifyStarted()
	{
		if (!IsAvailable()) { return; }
		StartedCount.fetch_add(1, std::memory_order_acq_rel);
		PCGEX_MULTI_LOG(LogTemp, Warning, TEXT("IAsyncMultiHandle[#%d|%s]::NotifyStarted = %d"), HandleIdx, *DEBUG_HandleId(), StartedCount.load());
	}

	void IAsyncMultiHandle::NotifyCompleted()
	{
		if (!IsAvailable()) { return; }
		CompletedCount.fetch_add(1, std::memory_order_acq_rel);
		PCGEX_MULTI_LOG(LogTemp, Warning, TEXT("IAsyncMultiHandle[#%d|%s]::NotifyCompleted = %d"), HandleIdx, *DEBUG_HandleId(), CompletedCount.load());
		CheckCompletion();
	}

	void IAsyncMultiHandle::CheckCompletion()
	{
		// Don't check if already ended
		if (GetState() == EAsyncHandleState::Ended) { return; }

		const int32 Expected = ExpectedCount.load(std::memory_order_acquire);
		const int32 Started = StartedCount.load(std::memory_order_acquire);
		const int32 Completed = CompletedCount.load(std::memory_order_acquire);

		PCGEX_MULTI_LOG(
			LogTemp, Warning, TEXT("IAsyncMultiHandle[#%d|%s]::CheckCompletion Expected : %d, Started : %d, Completed : %d"),
			HandleIdx, *DEBUG_HandleId(),
			Expected, Started, Completed);

		// Complete when all started tasks have finished
		// (Completed == Started ensures we wait for running tasks even if cancelled)
		if (Completed >= Expected && Completed == Started && Expected > 0)
		{
			Complete();
		}
	}

	void IAsyncMultiHandle::StartHandlesBatchImpl(const TArray<TSharedPtr<FTask>>& InHandles)
	{
		if (!IsAvailable() || InHandles.Num() == 0) { return; }

		RegisterExpected(InHandles.Num());
		for (const TSharedPtr<FTask>& Task : InHandles) { Launch(Task, true); }
	}

	bool IAsyncMultiHandle::IsAvailable() const
	{
		if (IsCancelled() || GetState() == EAsyncHandleState::Ended) { return false; }
		if (const TSharedPtr<IAsyncMultiHandle> RootPtr = Root.Pin()) { return RootPtr->IsAvailable(); }
		return true; // Root itself
	}

	void IAsyncMultiHandle::LaunchTask(const TSharedPtr<FTask>& InTask)
	{
		if (!IsAvailable()) { return; }

		PCGEX_MULTI_LOG(LogTemp, Warning, TEXT("IAsyncMultiHandle[#%d|%s]::LaunchTask"), HandleIdx, *DEBUG_HandleId())

		if (const TSharedPtr<IAsyncMultiHandle> RootPtr = Root.Pin())
		{
			InTask->SetParent(SharedThis(this));
			RootPtr->LaunchTask(InTask);
		}
	}

	void IAsyncMultiHandle::OnEnd(const bool bWasCancelled)
	{
		PCGEX_MULTI_LOG(LogTemp, Warning, TEXT("IAsyncMultiHandle[#%d|%s]::OnEnd(%d)"), HandleIdx, *DEBUG_HandleId(), bWasCancelled)

		if (!bWasCancelled && OnCompleteCallback)
		{
			FCompletionCallback Callback = MoveTemp(OnCompleteCallback);
			Callback();
		}

		if (const TSharedPtr<IAsyncMultiHandle> Parent = ParentHandle.Pin())
		{
			// TaskGroups only notify their parent (NOT root to avoid double counting)
			Parent->NotifyCompleted();
		}
	}

	// FAsyncToken
	FAsyncToken::FAsyncToken(const TWeakPtr<IAsyncMultiHandle>& InHandle) : Handle(InHandle)
	{
		if (const TSharedPtr<IAsyncMultiHandle> Pinned = Handle.Pin())
		{
			PCGEX_TASK_LOG(LogTemp, Error, TEXT("FAsyncToken::FAsyncToken @#%d|%s"), Pinned->HandleIdx, *Pinned->DEBUG_HandleId());
			Pinned->RegisterExpected();
		}
	}

	FAsyncToken::~FAsyncToken() { Release(); }

	void FAsyncToken::Release()
	{
		bool Expected = false;
		if (bReleased.compare_exchange_strong(Expected, true, std::memory_order_acq_rel))
		{
			if (const TSharedPtr<IAsyncMultiHandle> Pinned = Handle.Pin())
			{
				Pinned->NotifyCompleted();
				Handle.Reset();
			}
		}
	}

	// FTaskManager
	FTaskManager::FTaskManager(FPCGExContext* InContext)
		: IAsyncMultiHandle(FName("ROOT")), Context(InContext), ContextHandle(InContext->GetOrCreateHandle())
	{
		WorkHandle = Context->GetWorkHandle();
	}

	FTaskManager::~FTaskManager()
	{
		Cancel();
	}

	bool FTaskManager::IsAvailable() const
	{
		return ContextHandle.IsValid() && WorkHandle.IsValid() &&
			GetState() != EAsyncHandleState::Ended && !IsCancelled();
	}

	bool FTaskManager::IsWaitingForTasks() const
	{
		return GetState() == EAsyncHandleState::Running;
	}

	bool FTaskManager::Start()
	{
		// Manager starts when first task registers
		if (TryTransitionState(EAsyncHandleState::Idle, EAsyncHandleState::Running))
		{
			PCGEX_MANAGER_LOG(LogTemp, Warning, TEXT("FTaskManager::Start"));
			Context->PauseContext();
			return true;
		}
		return GetState() == EAsyncHandleState::Running;
	}

	void FTaskManager::Cancel()
	{
		const bool IsResetting = bResetting.load(std::memory_order_acquire);

		// Cancel will call End if we're idling.
		IAsyncHandle::Cancel();

		// Cancel all registered handles
		TArray<TSharedPtr<IAsyncHandle>> HandlesToCancel;
		{
			FWriteScopeLock WriteLock(RegistryLock);
			Tokens.Empty(); // Revoke tokens

			HandlesToCancel.Reserve(Registry.Num() + Groups.Num());
			for (const TWeakPtr<IAsyncHandle>& Weak : Registry)
			{
				if (TSharedPtr<IAsyncHandle> Handle = Weak.Pin()) { HandlesToCancel.Add(Handle); }
			}

			for (const TSharedPtr<FTaskGroup>& Group : Groups) { HandlesToCancel.Add(Group); }
			Registry.Empty();
			Groups.Empty();
		}

		// Cancel outside lock
		for (const TSharedPtr<IAsyncHandle>& Handle : HandlesToCancel) { Handle->Cancel(); }

		/*
		// Wait for running tasks with timeout
		const double StartTime = FPlatformTime::Seconds();
		const double TimeoutSeconds = 10.0;

		while (IsWaitingForTasks())
		{
			if (FPlatformTime::Seconds() - StartTime > TimeoutSeconds)
			{
				PCGEX_MANAGER_LOG(LogPCGEx, Error, TEXT("FTaskManager::Cancel - Timeout waiting for tasks to complete"));
				break;
			}
			FPlatformProcess::Sleep(0.001f);
		}
		*/
	}

	void FTaskManager::Reset()
	{
		if (IsCancelled())
		{
			PCGEX_MANAGER_LOG(LogTemp, Error, TEXT("FTaskManager::Reset but manager was cancelled"));
			return;
		}

		bool Expected = false;
		if (bResetting.compare_exchange_strong(Expected, true, std::memory_order_acq_rel))
		{
			PCGEX_MANAGER_LOG(LogTemp, Warning, TEXT("FTaskManager::Reset"));

			Cancel();

			// Reset values
			ExpectedCount.store(0, std::memory_order_release);
			StartedCount.store(0, std::memory_order_release);
			CompletedCount.store(0, std::memory_order_release);
			bCancelled.store(false, std::memory_order_release);

			// Reset state
			State.store(EAsyncHandleState::Idle, std::memory_order_release);
			bResetting.store(false, std::memory_order_release);

			//Context->UnpauseContext();
		}
	}

	TSharedPtr<FTaskGroup> FTaskManager::TryCreateTaskGroup(const FName& InName)
	{
		if (!IsAvailable()) { return nullptr; }

		PCGEX_MANAGER_LOG(LogTemp, Warning, TEXT("FTaskManager::TryCreateTaskGroup (%s)"), *InName.ToString());

		PCGEX_MAKE_SHARED(NewGroup, FTaskGroup, InName)

		int32 Idx = -1;
		{
			FWriteScopeLock WriteLock(RegistryLock);
			Idx = Groups.Add(NewGroup) + 1;
		}

		PCGEX_SHARED_THIS_DECL
		if (NewGroup->SetRoot(ThisPtr, Idx * -1))
		{
			// TODO : Make group self-referencing shared ptr
			// .Rst on Complete/End
			NewGroup->SetParent(ThisPtr);
			NewGroup->Start();
			return NewGroup;
		}
		return nullptr;
	}

	bool FTaskManager::TryRegisterHandle(const TSharedPtr<IAsyncHandle>& InHandle)
	{
		if (!IsAvailable()) { return false; }

		int32 Idx = -1;
		{
			FWriteScopeLock WriteLock(RegistryLock);
			Idx = Registry.Add(InHandle);
		}

		if (InHandle->SetRoot(SharedThis(this), Idx))
		{
			Start(); // Ensure manager is running
			InHandle->Start();
			return true;
		}
		return false;
	}

	TWeakPtr<FAsyncToken> FTaskManager::TryCreateToken(const FName& InName)
	{
		if (!IsAvailable()) { return nullptr; }

		FWriteScopeLock WriteLock(RegistryLock);
		PCGEX_MAKE_SHARED(Token, FAsyncToken, SharedThis(this))
		return Tokens.Add_GetRef(Token);
	}

	void FTaskManager::LaunchTask(const TSharedPtr<FTask>& InTask)
	{
		if (!IsAvailable()) { return; }

		PCGEX_MANAGER_LOG(LogTemp, Warning, TEXT("FTaskManager::LaunchTask : [%d|%s]"), InTask->HandleIdx, *InTask->DEBUG_HandleId());

		int32 Idx = -1;
		{
			FWriteScopeLock WriteLock(RegistryLock);
			Idx = Registry.Add(InTask);
		}

		PCGEX_SHARED_THIS_DECL

		// TODO : We SetParent if none exist, need to make sure we don't set it "too late"
		// This ensure tracking of roaming tasks
		if (!InTask->ParentHandle.IsValid()) { InTask->SetParent(ThisPtr); }
		if (!InTask->SetRoot(ThisPtr, Idx)) { return; }

		Start(); // Ensure manager is running

		UE::Tasks::Launch(
				*InTask->DEBUG_HandleId(),
				[WeakManager = TWeakPtr<FTaskManager>(ThisPtr), Task = InTask]()
				{
					const TSharedPtr<FTaskManager> Manager = WeakManager.Pin();
					if (!Manager || !Manager->IsAvailable())
					{
						Task->Cancel();
						Task->Complete();
						return;
					}

					if (Task->Start())
					{
						Task->ExecuteTask(Manager);
						Task->Complete();
					}
				},
				WorkPriority
			);
	}

	void FTaskManager::OnEnd(const bool bWasCancelled)
	{
		IAsyncMultiHandle::OnEnd(bWasCancelled);
		//Context->UnpauseContext();
	}

	// FTaskGroup
	FTaskGroup::FTaskGroup(const FName InName) : IAsyncMultiHandle(InName)
	{
	}

	void FTaskGroup::StartIterations(const int32 MaxItems, const int32 ChunkSize, const bool bForceSingleThreaded, const bool bPreparationOnly)
	{
		if (!IsAvailable() || (!bPreparationOnly && !OnIterationCallback) || MaxItems <= 0) { return; }

		const int32 SanitizedChunk = FMath::Max(1, ChunkSize);

		if (bForceSingleThreaded)
		{
			const int32 NumScopes = SubLoopScopes(ScopeCache, MaxItems, SanitizedChunk);
			if (NumScopes == 0) { return; }

			RegisterExpected(NumScopes);
			if (OnPrepareSubLoopsCallback) { OnPrepareSubLoopsCallback(ScopeCache); }

			PCGEX_MAKE_SHARED(Task, FForceSingleThreadedScopeIterationTask, 0)
			Task->bPrepareOnly = bPreparationOnly;
			Launch(Task, true);
		}
		else
		{
			StartRanges<FScopeIterationTask>(MaxItems, SanitizedChunk, bPreparationOnly);
		}
	}

	void FTaskGroup::StartSubLoops(const int32 MaxItems, const int32 ChunkSize, const bool bForceSingleThreaded)
	{
		StartIterations(MaxItems, ChunkSize, bForceSingleThreaded, true);
	}

	void FTaskGroup::AddSimpleCallback(FSimpleCallback&& InCallback)
	{
		SimpleCallbacks.Add(InCallback);
	}

	void FTaskGroup::StartSimpleCallbacks()
	{
		const int32 Count = SimpleCallbacks.Num();
		if (Count == 0) { return; }

		RegisterExpected(Count);
		for (int i = 0; i < Count; i++)
		{
			PCGEX_MAKE_SHARED(Task, FSimpleCallbackTask, i)
			Launch(Task, true);
		}
	}

	void FTaskGroup::ExecScopeIteration(const FScope& Scope, const bool bPrepareOnly) const
	{
		if (!IsAvailable()) { return; }
		if (OnSubLoopStartCallback) { OnSubLoopStartCallback(Scope); }
		if (!bPrepareOnly) { PCGEX_SCOPE_LOOP(i) { OnIterationCallback(i, Scope); } }
	}

	void FTaskGroup::TriggerSimpleCallback(int32 Index)
	{
		if (!IsAvailable() || !SimpleCallbacks.IsValidIndex(Index)) { return; }
		SimpleCallbacks[Index]();
	}

	// Task implementations
	void FSimpleCallbackTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		if (const TSharedPtr<IAsyncMultiHandle> Parent = ParentHandle.Pin())
		{
			StaticCastSharedPtr<FTaskGroup>(Parent)->TriggerSimpleCallback(TaskIndex);
		}
	}

	void FScopeIterationTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		if (const TSharedPtr<IAsyncMultiHandle> Parent = ParentHandle.Pin())
		{
			StaticCastSharedPtr<FTaskGroup>(Parent)->ExecScopeIteration(Scope, bPrepareOnly);
		}
	}

	void FForceSingleThreadedScopeIterationTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		const TSharedPtr<IAsyncMultiHandle> Parent = ParentHandle.Pin();
		if (!Parent) { return; }

		const TSharedPtr<FTaskGroup> Group = StaticCastSharedPtr<FTaskGroup>(Parent);
		if (!Group->IsAvailable()) { return; }

		const FScope& Scope = Group->ScopeCache[TaskIndex];
		Group->ExecScopeIteration(Scope, bPrepareOnly);

		const int32 NextIdx = Scope.GetNextScopeIndex();
		if (Group->ScopeCache.IsValidIndex(NextIdx))
		{
			PCGEX_MAKE_SHARED(Task, FForceSingleThreadedScopeIterationTask, NextIdx)
			Task->bPrepareOnly = bPrepareOnly;
			Group->Launch(Task, true);
		}
	}

	void FDeferredCallbackTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		Callback();
	}

	// Main thread execution
	FExecuteOnMainThread::FExecuteOnMainThread()
	{
	}

	bool FExecuteOnMainThread::Start()
	{
		if (!IAsyncHandle::Start()) { return false; }
		Schedule();
		return true;
	}

	void FExecuteOnMainThread::Schedule()
	{
		if (IsCancelled() || GetState() != EAsyncHandleState::Running)
		{
			Complete();
			return;
		}

		PCGEX_SUBSYSTEM
		PCGExSubsystem->RegisterBeginTickAction(
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				PCGEX_SUBSYSTEM
				This->EndTime = PCGExSubsystem->GetEndTime();
				if (!This->Execute()) { This->Schedule(); }
				else { This->Complete(); }
			});
	}

	bool FExecuteOnMainThread::Execute()
	{
		return true; // Override in derived
	}

	void FExecuteOnMainThread::OnEnd(bool bWasCancelled)
	{
		if (!bWasCancelled && OnCompleteCallback) { OnCompleteCallback(); }
		IAsyncHandle::OnEnd(bWasCancelled);
	}

	bool FExecuteOnMainThread::ShouldStop()
	{
		return FPlatformTime::Seconds() > EndTime;
	}

	// FScopeLoopOnMainThread
	FScopeLoopOnMainThread::FScopeLoopOnMainThread(const int32 NumIterations)
		: Scope(FScope(0, NumIterations, 0))
	{
	}

	bool FScopeLoopOnMainThread::Start()
	{
		check(OnIterationCallback)
		return FExecuteOnMainThread::Start();
	}

	void FScopeLoopOnMainThread::Cancel()
	{
		FExecuteOnMainThread::Cancel();
		Complete();
	}

	bool FScopeLoopOnMainThread::Execute()
	{
		if (IsCancelled() || Scope.Start >= Scope.End) { return true; }

		FPCGExContext* InContext = nullptr;
		if (const TSharedPtr<IAsyncMultiHandle> RootPtr = Root.Pin())
		{
			if (TSharedPtr<FTaskManager> Manager = StaticCastSharedPtr<FTaskManager>(RootPtr))
			{
				if (Manager->IsAvailable()) { InContext = Manager->GetContext(); }
			}
		}

		if (!InContext) { return true; }

		PCGEX_SCOPE_LOOP(Index)
		{
			OnIterationCallback(Index, Scope);
			if (ShouldStop())
			{
				Scope.Start = Index + 1;
				Scope.LoopIndex++;
				return false;
			}
		}

		return true;
	}
}

#undef PCGEX_TASK_LOG
