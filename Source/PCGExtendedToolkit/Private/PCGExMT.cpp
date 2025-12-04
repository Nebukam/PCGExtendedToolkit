// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExMT.h"
#include "Tasks/Task.h"
#include "PCGExHelpers.h"
#include "PCGExContext.h"
#include "PCGExGlobalSettings.h"
#include "PCGExSettings.h"
#include "PCGExSubSystem.h"
#include "HAL/PlatformTime.h"

#define PCGEX_TASK_LOG(...) //UE_LOG(__VA_ARGS__)
#define PCGEX_MULTI_LOG(...) //UE_LOG(__VA_ARGS__)
#define PCGEX_MANAGER_LOG(...) //UE_LOG(__VA_ARGS__)

namespace PCGExMT
{
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

	// IAsyncHandle
	IAsyncHandle::~IAsyncHandle()
	{
		if (GetState() != EAsyncHandleState::Ended)
		{
			Cancel();
			Complete();
		}
	}

	bool IAsyncHandle::SetRoot(const TSharedPtr<FTaskManager>& InRoot, const int32 InHandleIdx)
	{
		if (!InRoot) { return false; }
		HandleIdx = InHandleIdx;
		Root = InRoot;
		//PCGEX_TASK_LOG(LogTemp, Warning, TEXT("IAsyncHandle[#%d|%s]::SetRoot to [#%d|%s]"), HandleIdx, *DEBUG_HandleId(), InRoot->HandleIdx, *InRoot->DEBUG_HandleId());
		return true;
	}

	void IAsyncHandle::SetParent(const TSharedPtr<IAsyncMultiHandle>& InParent)
	{
		check(!ParentHandle.IsValid());
		if (!InParent) { return; }

		ParentHandle = InParent;
		PCGEX_TASK_LOG(LogTemp, Warning, TEXT("IAsyncHandle[#%d|%s]::SetParent to [#%d|%s]"), HandleIdx, *DEBUG_HandleId(), InParent->HandleIdx, *InParent->DEBUG_HandleId());

		if (!bExpected)
		{
			IAsyncMultiHandle::FRegistrationGuard Guard(InParent);

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
	}

	void IAsyncHandle::Complete()
	{
		if (TryTransitionState(EAsyncHandleState::Running, EAsyncHandleState::Ended))
		{
			PCGEX_TASK_LOG(LogTemp, Warning, TEXT("IAsyncHandle[#%d|%s]::Complete"), HandleIdx, *DEBUG_HandleId());
			OnEnd(IsCancelled());
		}
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
			Parent->NotifyCompleted();
		}
	}

	// IAsyncMultiHandle
	IAsyncMultiHandle::IAsyncMultiHandle(const FName InName) : GroupName(InName)
	{
	}

	IAsyncMultiHandle::~IAsyncMultiHandle()
	{
	}

	bool IAsyncMultiHandle::RegisterExpected(int32 Count)
	{
		if (!IsAvailable()) { return false; }
		ExpectedCount.fetch_add(Count, std::memory_order_acq_rel);
		PCGEX_MULTI_LOG(LogTemp, Warning, TEXT("IAsyncMultiHandle[#%d|%s]::RegisterExpected +%d (%d)"), HandleIdx, *DEBUG_HandleId(), Count, ExpectedCount.load());
		return true;
	}

	void IAsyncMultiHandle::NotifyStarted()
	{
		//if (!IsAvailable()) { return; }
		StartedCount.fetch_add(1, std::memory_order_acq_rel);
		PCGEX_MULTI_LOG(LogTemp, Warning, TEXT("IAsyncMultiHandle[#%d|%s]::NotifyStarted++ (%d)"), HandleIdx, *DEBUG_HandleId(), StartedCount.load());
	}

	void IAsyncMultiHandle::NotifyCompleted()
	{
		//if (!IsAvailable()) { return; }

		CompletedCount.fetch_add(1, std::memory_order_acq_rel);
		PCGEX_MULTI_LOG(LogTemp, Warning, TEXT("IAsyncMultiHandle[#%d|%s]::NotifyCompleted++ (%d)"), HandleIdx, *DEBUG_HandleId(), CompletedCount.load());

		CheckCompletion();
	}

	void IAsyncMultiHandle::Launch(const TSharedPtr<FTask>& InTask, const bool bIsExpected)
	{
		InTask->bExpected = bIsExpected;
		LaunchInternal(InTask);
	}

	int32 IAsyncMultiHandle::Launch(const int32 Count, FCreateLaunchablePredicate&& Predicate)
	{
		if (!CanScheduleWork()) { return 0; }

		TArray<TSharedPtr<FTask>> Handles;
		Handles.Reserve(Count);

		for (int i = 0; i < Count; i++) { if (TSharedPtr<FTask> Task = Predicate(i)) { Handles.Add(Task); } }
		StartHandlesBatchImpl(Handles);

		return Handles.Num();
	}

	int32 IAsyncMultiHandle::RegisterTask(const TSharedPtr<IAsyncHandle>& InTask)
	{
		FWriteScopeLock WriteLock(RegistryLock);
		return Registry.Add(InTask);
	}

	void IAsyncMultiHandle::ClearRegistry(const bool bCancel)
	{
		if (bCancel)
		{
			TArray<TSharedPtr<IAsyncHandle>> HandlesToCancel;

			{
				FWriteScopeLock WriteLock(RegistryLock);
				HandlesToCancel.Reserve(Registry.Num());
				for (const TWeakPtr<IAsyncHandle>& Weak : Registry) { if (TSharedPtr<IAsyncHandle> Handle = Weak.Pin()) { HandlesToCancel.Add(Handle); } }
				Registry.Empty();
			}

			// Cancel outside locks
			for (const TSharedPtr<IAsyncHandle>& Handle : HandlesToCancel) { Handle->Cancel(); }
		}
		else
		{
			FWriteScopeLock WriteLock(RegistryLock);
			Registry.Empty();
		}
	}

	void IAsyncMultiHandle::CheckCompletion()
	{
		EAsyncHandleState CurrentState = GetState();
		if (CurrentState == EAsyncHandleState::Ended) { return; }

		// Block completion checks during registration
		if (PendingRegistrations.load(std::memory_order_acquire) > 0) { return; }

		// Memory fence ensures we see all completed registrations
		std::atomic_thread_fence(std::memory_order_seq_cst);

		const int32 Expected = ExpectedCount.load(std::memory_order_acquire);
		const int32 Started = StartedCount.load(std::memory_order_acquire);
		const int32 Completed = CompletedCount.load(std::memory_order_acquire);

		if (Completed >= Expected && Completed == Started && Expected > 0)
		{
			EAsyncHandleState RunningState = EAsyncHandleState::Running;
			if (State.compare_exchange_strong(RunningState, EAsyncHandleState::Ended, std::memory_order_acq_rel))
			{
				OnEnd(IsCancelled());
			}
		}
	}

	void IAsyncMultiHandle::StartHandlesBatchImpl(const TArray<TSharedPtr<FTask>>& InHandles)
	{
		if (!CanScheduleWork()) { return; }

		if (InHandles.IsEmpty())
		{
			AssertEmptyThread(0);
			return;
		}

		PCGEX_SHARED_THIS_DECL
		TSharedPtr<FTaskManager> RootPtr = Root.Pin();
		if (!RootPtr) { RootPtr = StaticCastSharedPtr<FTaskManager>(ThisPtr); }

		if (!RootPtr) { return; }

		{
			FWriteScopeLock WriteLock(RegistryLock);
			FRegistrationGuard Guard(ThisPtr);

			RegisterExpected(InHandles.Num());

			for (const TSharedPtr<FTask>& Task : InHandles)
			{
				Task->HandleIdx = Registry.Add(Task);
				Task->bExpected = true;
				Task->SetParent(ThisPtr);
			}
		}

		for (const TSharedPtr<FTask>& Task : InHandles) { RootPtr->LaunchInternal(Task); }
	}

	void IAsyncMultiHandle::AssertEmptyThread(const int32 MaxItems) const
	{
		const FTaskManager* RootPtr = Root.IsValid() ? Root.Pin().Get() : static_cast<const FTaskManager*>(this);
		UE_LOG(LogPCGEx, Error, TEXT("[%s >>] StartRanges: MaxItems = %i - Graph will never finish! Enable bAssertOnEmptyThread for stack trace."), RootPtr ? *GetNameSafe(RootPtr->GetContext()->GetInputSettings<UPCGExSettings>()) : TEXT("UNKNOWN NODE"), MaxItems);
		if (GetDefault<UPCGExGlobalSettings>()->bAssertOnEmptyThread) { ensure(false); }
	}

	bool IAsyncMultiHandle::IsAvailable() const
	{
		if (IsCancelled() || GetState() == EAsyncHandleState::Ended) { return false; }
		if (const TSharedPtr<FTaskManager> RootPtr = Root.Pin()) { return RootPtr->IsAvailable(); }
		return true; // Root itself
	}

	void IAsyncMultiHandle::Cancel()
	{
		bool Expected = false;
		if (!bCancelled.compare_exchange_strong(Expected, true, std::memory_order_acq_rel)) { return; }

		ClearRegistry(true);

		// Try to end immediately if idle
		if (TryTransitionState(EAsyncHandleState::Idle, EAsyncHandleState::Ended))
		{
			PCGEX_TASK_LOG(LogTemp, Warning, TEXT("IAsyncHandle[#%d|%s]::Cancel"), HandleIdx, *DEBUG_HandleId());
			OnEnd(true);
		}
	}

	bool IAsyncMultiHandle::CanScheduleWork() { return IsAvailable(); }

	void IAsyncMultiHandle::LaunchInternal(const TSharedPtr<FTask>& InTask)
	{
		if (!IsAvailable()) { return; }

		PCGEX_MULTI_LOG(LogTemp, Warning, TEXT("IAsyncMultiHandle[#%d|%s]::LaunchTask"), HandleIdx, *DEBUG_HandleId())

		if (const TSharedPtr<FTaskManager> RootPtr = Root.Pin())
		{
			// Register in this handle's registry
			const int32 Idx = RegisterTask(InTask);
			InTask->HandleIdx = Idx;
			InTask->SetParent(SharedThis(this));

			// Launch through root
			RootPtr->LaunchInternal(InTask);
		}
	}

	void IAsyncMultiHandle::OnEnd(const bool bWasCancelled)
	{
		const FTaskManager* RootPtr = Root.IsValid() ? Root.Pin().Get() : static_cast<const FTaskManager*>(this);
		PCGEX_MULTI_LOG(LogTemp, Warning, TEXT("IAsyncMultiHandle[[%s]#%d|%s]::OnEnd(%d)"), RootPtr ? *GetNameSafe(RootPtr->GetContext()->GetInputSettings<UPCGExSettings>()) : TEXT(""), HandleIdx, *DEBUG_HandleId(), bWasCancelled)

		// Clear registry to free memory
		ClearRegistry();

		if (!bWasCancelled && OnCompleteCallback)
		{
			FCompletionCallback LocalCallback = MoveTemp(OnCompleteCallback);
			LocalCallback();
		}

		if (const TSharedPtr<IAsyncMultiHandle> Parent = ParentHandle.Pin())
		{
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
		if (IsCancelled()) { return false; }

		// Auto-reset from Ended state - this allows reuse without explicit Reset calls
		EAsyncHandleState CurrentState = GetState();
		if (CurrentState == EAsyncHandleState::Ended)
		{
			Reset();
			CurrentState = EAsyncHandleState::Idle;
		}

		if (CurrentState == EAsyncHandleState::Idle)
		{
			if (TryTransitionState(EAsyncHandleState::Idle, EAsyncHandleState::Running))
			{
				PCGEX_MANAGER_LOG(LogTemp, Warning, TEXT("FTaskManager::Start"));
				return true;
			}
		}

		return GetState() == EAsyncHandleState::Running;
	}

	void FTaskManager::Cancel()
	{
		// Don't call regular flow on cancellation, this is scorched eath.
		// Execution has been cancelled, we just need to cancel all ongoing tasks

		const bool IsResetting = bResetting.load(std::memory_order_acquire);

		bool Expected = false;
		if (!bCancelled.compare_exchange_strong(Expected, true, std::memory_order_acq_rel)) { return; }

		ClearRegistry(true);
	}

	void FTaskManager::ClearGroups()
	{
		FWriteScopeLock WriteLock(GroupsLock);
		Tokens.Empty();
		Groups.Empty();
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

			{
				FWriteScopeLock WriteLock(RegistryLock);
				Registry.Empty();
			}

			{
				FWriteScopeLock WriteLock(GroupsLock);
				Tokens.Empty();
				Groups.Empty();
			}

			// Reset counters
			ExpectedCount.store(0, std::memory_order_release);
			StartedCount.store(0, std::memory_order_release);
			CompletedCount.store(0, std::memory_order_release);
			bCancelled.store(false, std::memory_order_release);

			State.store(EAsyncHandleState::Idle, std::memory_order_release);

			bResetting.store(false, std::memory_order_release);
		}
	}

	bool FTaskManager::CanScheduleWork()
	{
		return Start();
	}

	TSharedPtr<FTaskGroup> FTaskManager::TryCreateTaskGroup(const FName& InName, const TSharedPtr<IAsyncMultiHandle>& InParentHandle)
	{
		if (!CanScheduleWork()) { return nullptr; }

		PCGEX_MANAGER_LOG(LogTemp, Warning, TEXT("FTaskManager::TryCreateTaskGroup (%s)"), *InName.ToString());

		PCGEX_MAKE_SHARED(NewGroup, FTaskGroup, InName)


		int32 Idx = -1;
		{
			FWriteScopeLock WriteLock(GroupsLock);
			Idx = Groups.Add(NewGroup) + 1;
		}

		PCGEX_SHARED_THIS_DECL
		if (NewGroup->SetRoot(ThisPtr, Idx * -1))
		{
			NewGroup->SetParent(InParentHandle ? InParentHandle : ThisPtr);
			NewGroup->Start();
			return NewGroup;
		}

		return nullptr;
	}

	bool FTaskManager::TryRegisterHandle(const TSharedPtr<IAsyncHandle>& InHandle, const TSharedPtr<IAsyncMultiHandle>& InParentHandle)
	{
		if (!CanScheduleWork()) { return false; }

		const int32 Idx = RegisterTask(InHandle);

		PCGEX_SHARED_THIS_DECL
		if (InHandle->SetRoot(ThisPtr, Idx))
		{
			InHandle->SetParent(InParentHandle ? InParentHandle : ThisPtr);
			InHandle->Start();
			return true;
		}
		return false;
	}

	TWeakPtr<FAsyncToken> FTaskManager::TryCreateToken(const FName& InName)
	{
		if (!CanScheduleWork()) { return nullptr; }

		FWriteScopeLock WriteLock(GroupsLock);
		PCGEX_MAKE_SHARED(Token, FAsyncToken, SharedThis(this))
		return Tokens.Add_GetRef(Token);
	}

	void FTaskManager::LaunchInternal(const TSharedPtr<FTask>& InTask)
	{
		if (!CanScheduleWork()) { return; }

		PCGEX_MANAGER_LOG(LogTemp, Warning, TEXT("FTaskManager::LaunchTask : [%d|%s]"), InTask->HandleIdx, *InTask->DEBUG_HandleId());
		PCGEX_SHARED_THIS_DECL

		// If task doesn't have a parent, register with root
		if (!InTask->ParentHandle.IsValid())
		{
			InTask->HandleIdx = RegisterTask(InTask);
			InTask->SetParent(ThisPtr);
		}

		// Set root if not set
		if (!InTask->Root.IsValid()) { InTask->SetRoot(ThisPtr, InTask->HandleIdx); }

		UE::Tasks::Launch(
				*InTask->DEBUG_HandleId(),
				[WeakManager = TWeakPtr<FTaskManager>(SharedThis(this)), Task = InTask]()
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
				}
			);
	}

	void FTaskManager::OnEnd(const bool bWasCancelled)
	{
		const FTaskManager* RootPtr = this;
		PCGEX_MULTI_LOG(LogTemp, Warning, TEXT("FTaskManager[[%s]#%d|%s]::OnEnd(%d)"), RootPtr ? *GetNameSafe(RootPtr->GetContext()->GetInputSettings<UPCGExSettings>()) : TEXT(""), HandleIdx, *DEBUG_HandleId(), bWasCancelled)

		// Clear registries
		ClearRegistry();

		// For the root manager, we DON'T call parent notification (there is no parent)
		// We call OnEndCallback directly, which notifies the context

		// Run the completion callback first (for consistency with IAsyncMultiHandle)
		if (!bWasCancelled && OnCompleteCallback)
		{
			const FCompletionCallback Callback = MoveTemp(OnCompleteCallback);
			Callback();
		}

		// Then the end callback (context notification)
		if (OnEndCallback) { OnEndCallback(bWasCancelled); }
	}

	void FTaskManager::ClearRegistry(const bool bCancel)
	{
		if (bCancel)
		{
			// Cancel all registered handles
			TArray<TSharedPtr<IAsyncHandle>> HandlesToCancel;
			{
				FWriteScopeLock WriteLock(RegistryLock);

				HandlesToCancel.Reserve(Registry.Num() + Groups.Num());
				for (const TWeakPtr<IAsyncHandle>& Weak : Registry) { if (TSharedPtr<IAsyncHandle> Handle = Weak.Pin()) { HandlesToCancel.Add(Handle); } }

				Registry.Empty();
			}

			{
				FWriteScopeLock WriteLock(GroupsLock);
				Tokens.Empty();

				for (const TSharedPtr<FTaskGroup>& Group : Groups) { HandlesToCancel.Add(Group); }
				Groups.Empty();
			}

			// Cancel outside locks
			for (const TSharedPtr<IAsyncHandle>& Handle : HandlesToCancel) { Handle->Cancel(); }
		}
		else
		{
			{
				FWriteScopeLock WriteLock(GroupsLock);
				Tokens.Empty();
				Groups.Empty();
			}

			IAsyncMultiHandle::ClearRegistry(false);
		}
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
			if (!NumScopes)
			{
				AssertEmptyThread(MaxItems);
				return;
			}

			{
				FRegistrationGuard Guard(SharedThis(this));

				RegisterExpected(NumScopes);
				if (OnPrepareSubLoopsCallback) { OnPrepareSubLoopsCallback(ScopeCache); }

				// TODO : Refactor this so we register all tasks in advance, and each task has a reference to the next one
				// so we don't have to cache scoping
				PCGEX_MAKE_SHARED(Task, FForceSingleThreadedScopeIterationTask, 0)
				Task->bPrepareOnly = bPreparationOnly;
				Launch(Task, true);
			}
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
		TArray<TSharedPtr<FTask>> Tasks;
		Tasks.Reserve(Count);

		for (int i = 0; i < Count; i++)
		{
			PCGEX_MAKE_SHARED(Task, FSimpleCallbackTask, i)
			Tasks.Add(Task);
		}

		StartHandlesBatchImpl(Tasks);
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

	void FTask::Launch(const TSharedPtr<FTask>& InTask, const bool bIsExpected) const
	{
		if (const TSharedPtr<IAsyncMultiHandle> Parent = ParentHandle.Pin()) { Parent->Launch(InTask, bIsExpected); }
		else if (const TSharedPtr<FTaskManager> RootPtr = Root.Pin()) { RootPtr->Launch(InTask, bIsExpected); }
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
		if (!bWasCancelled && OnCompleteCallback)
		{
			const FCompletionCallback Callback = MoveTemp(OnCompleteCallback);
			Callback();
		}
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
		if (const TSharedPtr<FTaskManager> RootPtr = Root.Pin())
		{
			if (RootPtr->IsAvailable()) { InContext = RootPtr->GetContext(); }
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
