// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExMT.h"
#include "Tasks/Task.h"
#include "PCGExHelpers.h"
#include "PCGExContext.h"
#include "PCGExGlobalSettings.h"
#include "PCGExSubSystem.h"
#include "HAL/PlatformTime.h"

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
		case EPCGExAsyncPriority::Normal: Priority = UE::Tasks::ETaskPriority::Normal; break;
		case EPCGExAsyncPriority::High: Priority = UE::Tasks::ETaskPriority::High; break;
		case EPCGExAsyncPriority::BackgroundHigh: Priority = UE::Tasks::ETaskPriority::BackgroundHigh; break;
		case EPCGExAsyncPriority::BackgroundNormal: Priority = UE::Tasks::ETaskPriority::BackgroundNormal; break;
		case EPCGExAsyncPriority::BackgroundLow: Priority = UE::Tasks::ETaskPriority::BackgroundLow; break;
		}
	}

	FScope::FScope(const int32 InStart, const int32 InCount, const int32 InLoopIndex)
		: Start(InStart), Count(InCount), End(InStart + InCount), LoopIndex(InLoopIndex) {}

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
		UE_LOG(LogPCGEx, Error, TEXT("StartRanges: MaxItems = %i - Graph will never finish! Enable bAssertOnEmptyThread for stack trace."), MaxItems);
		if (GetDefault<UPCGExGlobalSettings>()->bAssertOnEmptyThread) { ensure(false); }
	}

	// FAsyncHandle
	FAsyncHandle::~FAsyncHandle()
	{
		if (GetState() != EAsyncHandleState::Ended) { Cancel(); Complete(); }
	}

	bool FAsyncHandle::SetRoot(const TSharedPtr<FAsyncMultiHandle>& InRoot, const int32 InHandleIdx)
	{
		if (!InRoot) { return false; }
		HandleIdx = InHandleIdx;
		Root = InRoot;
		return InRoot->RegisterExpected();
	}

	void FAsyncHandle::SetParent(const TSharedPtr<FAsyncMultiHandle>& InParent)
	{
		if (!InParent) { return; }
		ParentHandle = InParent;
		InParent->RegisterExpected();
	}

	bool FAsyncHandle::Start()
	{
		if (!TryTransitionState(EAsyncHandleState::Idle, EAsyncHandleState::Running))
		{
			Cancel(); Complete();
			return false;
		}

		if (const TSharedPtr<FAsyncMultiHandle> Parent = ParentHandle.Pin()) { Parent->NotifyStarted(); }
		if (const TSharedPtr<FAsyncMultiHandle> RootPtr = Root.Pin()) { RootPtr->NotifyStarted(); }
		return true;
	}

	void FAsyncHandle::Cancel()
	{
		bool Expected = false;
		if (!bCancelled.compare_exchange_strong(Expected, true, std::memory_order_acq_rel)) { return; }

		// Try to end immediately if idle
		if (TryTransitionState(EAsyncHandleState::Idle, EAsyncHandleState::Ended))
		{
			OnEnd(true);
		}
	}

	void FAsyncHandle::Complete()
	{
		if (TryTransitionState(EAsyncHandleState::Running, EAsyncHandleState::Ended))
		{
			OnEnd(IsCancelled());
		}
	}

	bool FAsyncHandle::TryTransitionState(EAsyncHandleState From, EAsyncHandleState To)
	{
		return State.compare_exchange_strong(From, To, std::memory_order_acq_rel);
	}

	void FAsyncHandle::OnEnd(const bool bWasCancelled)
	{
		UE_LOG(LogTemp, Warning, TEXT("FAsyncHandle::OnEnd"))
		if (const TSharedPtr<FAsyncMultiHandle> Parent = ParentHandle.Pin()) { Parent->NotifyCompleted(); }
		if (const TSharedPtr<FAsyncMultiHandle> RootPtr = Root.Pin()) { RootPtr->NotifyCompleted(); }
	}

	// FAsyncMultiHandle
	FAsyncMultiHandle::FAsyncMultiHandle(const FName InName) : GroupName(InName) {}

	FAsyncMultiHandle::~FAsyncMultiHandle()
	{
		if (GetState() != EAsyncHandleState::Ended) { Cancel(); Complete(); }
	}

	bool FAsyncMultiHandle::RegisterExpected(int32 Count)
	{
		if (!IsAvailable()) { return false; }
		ExpectedCount.fetch_add(Count, std::memory_order_acq_rel);
		return true;
	}

	void FAsyncMultiHandle::NotifyStarted()
	{
		if (!IsAvailable()) { return; }
		StartedCount.fetch_add(1, std::memory_order_acq_rel);
	}

	void FAsyncMultiHandle::NotifyCompleted()
	{
		UE_LOG(LogTemp, Warning, TEXT("FAsyncMultiHandle::NotifyCompleted"))
		const int32 Completed = CompletedCount.fetch_add(1, std::memory_order_acq_rel) + 1;
		CheckCompletion();
	}

	void FAsyncMultiHandle::CheckCompletion()
	{
		const int32 Expected = ExpectedCount.load(std::memory_order_acquire);
		const int32 Started = StartedCount.load(std::memory_order_acquire);
		const int32 Completed = CompletedCount.load(std::memory_order_acquire);

		// All expected tasks have started and completed
		if (Completed >= Expected && Completed == Started && Expected > 0)
		{
			Complete();
		}
	}

	bool FAsyncMultiHandle::IsAvailable() const
	{
		if (IsCancelled() || GetState() == EAsyncHandleState::Ended) { return false; }
		if (const TSharedPtr<FAsyncMultiHandle> RootPtr = Root.Pin())
		{
			return RootPtr->IsAvailable();
		}
		return true; // Root itself
	}

	void FAsyncMultiHandle::LaunchTask(const TSharedPtr<FTask>& InTask)
	{
		if (!IsAvailable()) { return; }

		if (const TSharedPtr<FAsyncMultiHandle> RootPtr = Root.Pin())
		{
			InTask->SetParent(SharedThis(this));
			RootPtr->LaunchTask(InTask);
		}
	}

	void FAsyncMultiHandle::OnEnd(const bool bWasCancelled)
	{
		if (!bWasCancelled && OnCompleteCallback)
		{
			FCompletionCallback Callback = MoveTemp(OnCompleteCallback);
			Callback();
		}
		UE_LOG(LogTemp, Warning, TEXT("FAsyncMultiHandle::OnEnd"))
		FAsyncHandle::OnEnd(bWasCancelled);
	}

	// FAsyncToken
	FAsyncToken::FAsyncToken(const TWeakPtr<FAsyncMultiHandle>& InHandle) : Handle(InHandle)
	{
		if (const TSharedPtr<FAsyncMultiHandle> Pinned = Handle.Pin()) { Pinned->RegisterExpected(); }
	}

	FAsyncToken::~FAsyncToken() { Release(); }

	void FAsyncToken::Release()
	{
		bool Expected = false;
		if (bReleased.compare_exchange_strong(Expected, true, std::memory_order_acq_rel))
		{
			if (const TSharedPtr<FAsyncMultiHandle> Pinned = Handle.Pin())
			{
				Pinned->NotifyCompleted();
				Handle.Reset();
			}
		}
	}

	// FTaskManager
	FTaskManager::FTaskManager(FPCGExContext* InContext)
		: FAsyncMultiHandle(FName("ROOT")), Context(InContext), ContextHandle(InContext->GetOrCreateHandle())
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
			Context->PauseContext();
			return true;
		}
		return GetState() == EAsyncHandleState::Running;
	}

	void FTaskManager::Cancel()
	{
		FAsyncHandle::Cancel();

		// Cancel all registered handles
		TArray<TSharedPtr<FAsyncHandle>> HandlesToCancel;
		{
			FWriteScopeLock WriteLock(RegistryLock);
			Tokens.Empty(); // Revoke tokens
			
			HandlesToCancel.Reserve(Registry.Num());
			for (const TWeakPtr<FAsyncHandle>& Weak : Registry)
			{
				if (TSharedPtr<FAsyncHandle> Handle = Weak.Pin()) { HandlesToCancel.Add(Handle); }
			}
			Registry.Empty();
		}

		// Cancel outside lock
		for (const TSharedPtr<FAsyncHandle>& Handle : HandlesToCancel) { Handle->Cancel(); }

		// Wait for running tasks
		while (IsWaitingForTasks())
		{
			FPlatformProcess::Sleep(0.001f);
		}
	}

	void FTaskManager::Reset()
	{
		if (!IsAvailable()) { return; }

		Cancel();
		
		// Reset state
		ExpectedCount.store(0, std::memory_order_release);
		StartedCount.store(0, std::memory_order_release);
		CompletedCount.store(0, std::memory_order_release);
		bCancelled.store(false, std::memory_order_release);
		State.store(EAsyncHandleState::Idle, std::memory_order_release);

		Context->UnpauseContext();
	}

	TSharedPtr<FTaskGroup> FTaskManager::TryCreateTaskGroup(const FName& InName)
	{
		if (!IsAvailable()) { return nullptr; }

		PCGEX_MAKE_SHARED(NewGroup, FTaskGroup, InName)
		if (NewGroup->SetRoot(SharedThis(this), -1))
		{
			UE_LOG(LogTemp, Warning, TEXT("FTaskManager::TryCreateTaskGroup %s"), *InName.ToString())
			NewGroup->Start();
			return NewGroup;
		}
		return nullptr;
	}

	bool FTaskManager::TryRegisterHandle(const TSharedPtr<FAsyncHandle>& InHandle)
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

		int32 Idx = -1;
		{
			FWriteScopeLock WriteLock(RegistryLock);
			Idx = Registry.Add(InTask);
		}

		if (!InTask->SetRoot(SharedThis(this), Idx)) { return; }

		Start(); // Ensure manager is running

		UE_LOG(LogTemp, Warning, TEXT("LaunchTask"))
		
		PCGEX_SHARED_THIS_DECL
		UE::Tasks::Launch(
			*InTask->HandleId(),
			[WeakManager = TWeakPtr<FTaskManager>(ThisPtr), Task = InTask]()
			{
				const TSharedPtr<FTaskManager> Manager = WeakManager.Pin();
				if (!Manager || !Manager->IsAvailable()) { return; }

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
		FAsyncMultiHandle::OnEnd(bWasCancelled);
		UE_LOG(LogTemp, Warning, TEXT("FTaskManager::OnEnd"))
		Context->UnpauseContext();
	}

	// FTaskGroup
	FTaskGroup::FTaskGroup(const FName InName) : FAsyncMultiHandle(InName) {}

	void FTaskGroup::StartIterations(const int32 MaxItems, const int32 ChunkSize, const bool bForceSingleThreaded)
	{
		if (!IsAvailable() || !OnIterationCallback || MaxItems <= 0) { return; }

		const int32 SanitizedChunk = FMath::Max(1, ChunkSize);

		if (bForceSingleThreaded)
		{
			const int32 NumScopes = SubLoopScopes(ScopeCache, MaxItems, SanitizedChunk);
			if (NumScopes == 0) { return; }

			RegisterExpected(NumScopes);
			if (OnPrepareSubLoopsCallback) { OnPrepareSubLoopsCallback(ScopeCache); }

			PCGEX_MAKE_SHARED(Task, FForceSingleThreadedScopeIterationTask, 0)
			Task->bPrepareOnly = false;
			Launch(Task);
		}
		else
		{
			StartRanges<FScopeIterationTask>(MaxItems, SanitizedChunk, false);
		}
	}

	void FTaskGroup::StartSubLoops(const int32 MaxItems, const int32 ChunkSize, const bool bForceSingleThreaded)
	{
		if (bForceSingleThreaded)
		{
			if (!IsAvailable() || MaxItems <= 0) { return; }

			const int32 NumScopes = SubLoopScopes(ScopeCache, MaxItems, FMath::Max(1, ChunkSize));
			if (NumScopes == 0) { return; }

			RegisterExpected(NumScopes);
			if (OnPrepareSubLoopsCallback) { OnPrepareSubLoopsCallback(ScopeCache); }

			PCGEX_MAKE_SHARED(Task, FForceSingleThreadedScopeIterationTask, 0)
			Task->bPrepareOnly = true;
			Launch(Task);
		}
		else
		{
			StartRanges<FScopeIterationTask>(MaxItems, ChunkSize, true);
		}
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
			Launch(Task);
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
		if (const TSharedPtr<FAsyncMultiHandle> Parent = ParentHandle.Pin())
		{
			StaticCastSharedPtr<FTaskGroup>(Parent)->TriggerSimpleCallback(TaskIndex);
		}
	}

	void FScopeIterationTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		if (const TSharedPtr<FAsyncMultiHandle> Parent = ParentHandle.Pin())
		{
			StaticCastSharedPtr<FTaskGroup>(Parent)->ExecScopeIteration(Scope, bPrepareOnly);
		}
	}

	void FForceSingleThreadedScopeIterationTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		const TSharedPtr<FAsyncMultiHandle> Parent = ParentHandle.Pin();
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
			Group->Launch(Task);
		}
	}

	void FDeferredCallbackTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		Callback();
	}

	// Main thread execution
	FExecuteOnMainThread::FExecuteOnMainThread() {}

	bool FExecuteOnMainThread::Start()
	{
		if (!FAsyncHandle::Start()) { return false; }
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
		FAsyncHandle::OnEnd(bWasCancelled);
	}

	bool FExecuteOnMainThread::ShouldStop()
	{
		return FPlatformTime::Seconds() > EndTime;
	}

	// FScopeLoopOnMainThread
	FScopeLoopOnMainThread::FScopeLoopOnMainThread(const int32 NumIterations)
		: Scope(FScope(0, NumIterations, 0)) {}

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
		if (const TSharedPtr<FAsyncMultiHandle> RootPtr = Root.Pin())
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