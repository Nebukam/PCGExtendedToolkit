// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "PCGExMT.h"


#include "Tasks/Task.h"

namespace PCGExMT
{
	FAsyncHandle::~FAsyncHandle()
	{
		Cancel(); // Safety first
	}

	void FAsyncHandle::SetRoot(const TSharedPtr<FAsyncMultiHandle>& InRoot, const int32 InHandleIdx)
	{
		HandleIdx = InHandleIdx;
		Root = InRoot;
		InRoot->IncrementPendingTasks();
	}

	void FAsyncHandle::SetParent(const TSharedPtr<FAsyncMultiHandle>& InParent)
	{
		if (!InParent) { return; }
		ParentHandle = InParent;
		InParent->IncrementPendingTasks();
	}

	bool FAsyncHandle::Start()
	{
		EAsyncHandleState ExpectedState = EAsyncHandleState::Idle;
		CompareAndSetState(ExpectedState, EAsyncHandleState::Running);
		return GetState() == EAsyncHandleState::Running;
	}

	bool FAsyncHandle::Cancel()
	{
		bool bExpected = false;
		if (bIsCancelled.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel))
		{
			// Task can be tentatively ended
			EAsyncHandleState ExpectedState = EAsyncHandleState::Idle;
			if (CompareAndSetState(ExpectedState, EAsyncHandleState::Ended))
			{
				// Task was idle, can be ended
				End(true);
			}
		}
		else
		{
			// Otherwise, the task was already marked as cancelled
		}

		// Return whether we're ended or waiting on completion (running)
		return GetState() == EAsyncHandleState::Ended;
	}

	bool FAsyncHandle::Complete()
	{
		EAsyncHandleState ExpectedState = EAsyncHandleState::Running;
		if (CompareAndSetState(ExpectedState, EAsyncHandleState::Ended))
		{
			// Task was running, assume proper ending
			End(IsCancelled());
		}

		return GetState() == EAsyncHandleState::Ended;
	}

	void FAsyncHandle::End(const bool bIsCancellation)
	{
		if (const TSharedPtr<FAsyncMultiHandle> PinnedRoot = Root.Pin()) { PinnedRoot->IncrementCompletedTasks(); }
		if (const TSharedPtr<FAsyncMultiHandle> PinnedParent = ParentHandle.Pin()) { PinnedParent->IncrementCompletedTasks(); }
	}

	bool FAsyncHandle::CompareAndSetState(EAsyncHandleState& ExpectedState, EAsyncHandleState NewState)
	{
		return State.compare_exchange_strong(ExpectedState, NewState, std::memory_order_acq_rel);
	}

	FAsyncMultiHandle::FAsyncMultiHandle(const bool InForceSync, const FName InName)
		: bForceSync(InForceSync), GroupName(InName)
	{
	}

	FAsyncMultiHandle::~FAsyncMultiHandle()
	{
		// Try cancel first or force-complete
		if (!Cancel()) { Complete(); }
	}

	void FAsyncMultiHandle::SetRoot(const TSharedPtr<FAsyncMultiHandle>& InRoot, const int32 InHandleIdx)
	{
		bForceSync = InRoot->bForceSync;
		FAsyncHandle::SetRoot(InRoot, InHandleIdx);
	}

	void FAsyncMultiHandle::IncrementPendingTasks()
	{
		PendingTaskCount.fetch_add(1, std::memory_order_release);
		HandleTaskStart();
	}

	void FAsyncMultiHandle::IncrementCompletedTasks()
	{
		const int32 MinCount = ExpectedTaskCount.load(std::memory_order_acquire);
		const int32 CompletedCount = CompletedTaskCount.fetch_add(1, std::memory_order_acquire) + 1; // + 1 because fetch_add returns the previous value
		const int32 StartedCount = PendingTaskCount.load(std::memory_order_acquire);

		if (CompletedCount >= MinCount && CompletedCount == StartedCount) { Complete(); }
	}

	bool FAsyncMultiHandle::IsAvailable() const
	{
		if (IsCancelled()) { return false; }
		if (const TSharedPtr<FAsyncMultiHandle> PinnedRoot = Root.Pin()) { if (!PinnedRoot->IsAvailable()) { return false; } }
		else { return false; }
		return true;
	}

	void FAsyncMultiHandle::HandleTaskStart()
	{
	}

	void FAsyncMultiHandle::StartBackgroundTask(const TSharedPtr<FTask>& InTask)
	{
		if (!IsAvailable()) { return; }

		if (const TSharedPtr<FAsyncMultiHandle> PinnedRoot = Root.Pin())
		{
			// Register to self first
			InTask->SetParent(SharedThis(this));

			// Then push to root
			PinnedRoot->StartBackgroundTask(InTask);
		}
	}

	void FAsyncMultiHandle::StartSynchronousTask(const TSharedPtr<FTask>& InTask)
	{
		if (!IsAvailable()) { return; }

		if (const TSharedPtr<FAsyncMultiHandle> PinnedRoot = Root.Pin())
		{
			// Register to self first
			InTask->SetParent(SharedThis(this));

			// Then push to root
			PinnedRoot->StartSynchronousTask(InTask);
		}
	}

	void FAsyncMultiHandle::End(const bool bIsCancellation)
	{
		// Complete callback before notifying hierarchy
		if (!bIsCancellation && OnCompleteCallback)
		{
			FCompletionCallback Callback = MoveTemp(OnCompleteCallback);
			Callback();
		}

		FAsyncHandle::End(bIsCancellation);
	}

	void FAsyncMultiHandle::Reset()
	{
		bIsCancelled.store(false, std::memory_order_release);
		PendingTaskCount.store(0, std::memory_order_release);
		CompletedTaskCount.store(0, std::memory_order_release);
		SetState(EAsyncHandleState::Idle);
	}

	FAsyncToken::FAsyncToken(const TWeakPtr<FAsyncMultiHandle>& InHandle, const FName& InName):
		Handle(InHandle), Name(InName)
	{
		if (const TSharedPtr<FAsyncMultiHandle> PinnedHandle = Handle.Pin()) { PinnedHandle->IncrementPendingTasks(); }
	}

	FAsyncToken::~FAsyncToken()
	{
		if (const TSharedPtr<FAsyncMultiHandle> PinnedHandle = Handle.Pin()) { PinnedHandle->IncrementCompletedTasks(); }
	}

	void FAsyncToken::Release()
	{
		bool Expected = false;
		if (bIsReleased.compare_exchange_strong(Expected, true, std::memory_order_acq_rel))
		{
			if (const TSharedPtr<FAsyncMultiHandle> PinnedHandle = Handle.Pin())
			{
				PinnedHandle->IncrementCompletedTasks();
				Handle.Reset();
			}
		}
	}

	FTaskManager::FTaskManager(FPCGExContext* InContext, const bool InForceSync)
		: FAsyncMultiHandle(InForceSync, FName("ROOT")), Context(InContext)
	{
		PCGEX_LOG_CTR(FTaskManager)
		WorkPermit = Context->GetWorkPermit();
	}

	FTaskManager::~FTaskManager()
	{
		PCGEX_LOG_DTR(FTaskManager)
		Cancel();
	}

	bool FTaskManager::IsAvailable() const
	{
		return (!IsCancelling() && !IsCancelled() && !IsResetting() && WorkPermit.IsValid());
	}

	bool FTaskManager::IsWaitingForRunningTasks() const
	{
		return !bForceSync && GetState() == EAsyncHandleState::Running;
	}

	bool FTaskManager::Start()
	{
		// Called whenever a handle registers running work to root
		// Normally nothing would trigger here
		if (!IsAvailable())
		{
			UE_LOG(LogTemp, Error, TEXT("Attempting to register async work while manager is unavailable. If there's a crash, it's probably because this happened."));
			return false;
		}

		Context->PauseContext();
		SetState(EAsyncHandleState::Running);

		return true;
	}

	bool FTaskManager::Cancel()
	{
		bool Expected = false;
		if (!bIsCancelling.compare_exchange_strong(Expected, true, std::memory_order_acq_rel))
		{
			// Already cancelling
			return false;
		}

		Expected = false;
		if (!bIsCancelled.compare_exchange_strong(Expected, true, std::memory_order_acq_rel))
		{
			// Already cancelled
			bIsCancelling.store(false, std::memory_order_release);
			return false;
		}

		{
			FWriteScopeLock WriteTokenLock(TokensLock);
			FWriteScopeLock WriteTasksLock(TasksLock);
			FWriteScopeLock WriteGroupsLock(GroupsLock);

			Tokens.Empty(); // Revoke all tokens

			// Cancel groups & tasks
			Groups.Empty();
			for (const TWeakPtr<FTask>& WeakTask : Tasks) { if (const TSharedPtr<FTask> Task = WeakTask.Pin()) { Task->Cancel(); } }
			Tasks.Empty();
		}

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExTaskManager::WaitingForRunningTasks);

			// Fail safe for tasks that cannot be cancelled mid-way
			// This will only be false in cases where lots of regen/cancel happen in the same frame.
			while (IsWaitingForRunningTasks())
			{
				// (；′⌒`)
			}
		}

		Tokens.Empty();
		Groups.Empty();

		bIsCancelling.store(false, std::memory_order_release);
		return true;
	}

	void FTaskManager::Reset()
	{
		if (!IsAvailable()) { return; } // Don't reset if we're already cancelled

		bIsResetting.store(true, std::memory_order_release);

		Cancel();                   // Cancel ongoing work first, just in case
		FAsyncMultiHandle::Reset(); // Reset trackers, cancellation, set State to Idle

		bIsResetting.store(false, std::memory_order_release);

		Context->UnpauseContext(); // Safety unpause
	}

	void FTaskManager::ReserveTasks(const int32 NumTasks)
	{
		FWriteScopeLock WriteLock(TasksLock);
		Tasks.Reserve(Tasks.Num() + NumTasks);
	}

	TSharedPtr<FTaskGroup> FTaskManager::TryCreateTaskGroup(const FName& InName)
	{
		FWriteScopeLock WriteGroupsLock(GroupsLock);

		if (!IsAvailable()) { return nullptr; }

		PCGEX_MAKE_SHARED(NewGroup, FTaskGroup, bForceSync, InName)
		NewGroup->SetRoot(SharedThis(this), -1);
		NewGroup->Start(); // So its state can be updated properly

		return Groups.Add_GetRef(NewGroup);
	}

	TWeakPtr<FAsyncToken> FTaskManager::TryCreateToken(const FName& TokenName)
	{
		FWriteScopeLock WriteGroupsLock(TokensLock);

		if (!IsAvailable()) { return nullptr; }

		PCGEX_MAKE_SHARED(Token, FAsyncToken, SharedThis(this), TokenName)
		return Tokens.Add_GetRef(Token);
	}

	void FTaskManager::HandleTaskStart()
	{
		Start();
		FAsyncMultiHandle::HandleTaskStart();
	}

	void FTaskManager::End(const bool bIsCancellation)
	{
		FAsyncMultiHandle::End(bIsCancellation);
		Context->UnpauseContext();
	}

	void FTaskManager::StartBackgroundTask(const TSharedPtr<FTask>& InTask)
	{
		if (!IsAvailable()) { return; }

		int32 Idx = -1;
		{
			FWriteScopeLock WriteTasksLock(TasksLock);
			Idx = Tasks.Add(InTask);
		}

		TSharedPtr<FTaskManager> LocalManager = SharedThis(this);
		InTask->SetRoot(LocalManager, Idx);

		PCGEX_SHARED_THIS_DECL
		UE::Tasks::Launch(
				*InTask->HandleId(),
				[
					WeakManager = TWeakPtr<FTaskManager>(LocalManager),
					Task = InTask]()
				{
					const TSharedPtr<FTaskManager> Manager = WeakManager.Pin();
					if (!Manager || !Manager->IsAvailable()) { return; }

					if (Task->Start())
					{
						Task->ExecuteTask(Manager);
						Task->Complete();
					}

					// Cleanup task so we don't redundantly cancel it
					//if (Task->HandleIdx != -1) { Manager->Tasks[Task->HandleIdx] = nullptr; }
				},
				WorkPriority
			);
	}

	void FTaskManager::StartSynchronousTask(const TSharedPtr<FTask>& InTask)
	{
		if (!IsAvailable()) { return; }

		InTask->SetRoot(SharedThis(this));
		FAsyncMultiHandle::StartSynchronousTask(InTask);
	}

	FTaskGroup::FTaskGroup(const bool InForceSync, const FName InName)
		: FAsyncMultiHandle(InForceSync, InName)
	{
	}

	void FTaskGroup::StartIterations(const int32 MaxItems, const int32 ChunkSize, const bool bDaisyChain)
	{
		if (!IsAvailable() || !OnIterationCallback) { return; }

		check(MaxItems > 0);

		const int32 SanitizedChunkSize = FMath::Max(1, ChunkSize);

		if (bDaisyChain)
		{
			bDaisyChained = true;

			SetExpectedTaskCount(SubLoopScopes(Loops, MaxItems, SanitizedChunkSize));

			if (OnPrepareSubLoopsCallback) { OnPrepareSubLoopsCallback(Loops); }

			PCGEX_MAKE_SHARED(Task, FDaisyChainScopeIterationTask, 0)
			LaunchWithPreparation(Task, false);
		}
		else
		{
			StartRanges<FScopeIterationTask>(MaxItems, SanitizedChunkSize, false);
		}
	}

	void FTaskGroup::StartSubLoops(const int32 MaxItems, const int32 ChunkSize, const bool bDaisyChain)
	{
		if (!bDaisyChain)
		{
			StartRanges<FScopeIterationTask>(MaxItems, ChunkSize, true);
			return;
		}

		if (!IsAvailable()) { return; }

		const TSharedPtr<FAsyncMultiHandle> PinnedRoot = Root.Pin();
		if (!PinnedRoot) { return; }

		check(MaxItems > 0);

		// Compute sub scopes
		SetExpectedTaskCount(SubLoopScopes(Loops, MaxItems, FMath::Max(1, ChunkSize)));
		StaticCastSharedPtr<FTaskManager>(PinnedRoot)->ReserveTasks(Loops.Num());

		bDaisyChained = true;

		if (OnPrepareSubLoopsCallback) { OnPrepareSubLoopsCallback(Loops); }

		PCGEX_MAKE_SHARED(Task, FDaisyChainScopeIterationTask, 0)
		LaunchWithPreparation(Task, true);
	}

	void FTaskGroup::AddSimpleCallback(FSimpleCallback&& InCallback)
	{
		SimpleCallbacks.Add(InCallback);
	}

	void FTaskGroup::StartSimpleCallbacks()
	{
		SetExpectedTaskCount(SimpleCallbacks.Num());
		int Count = ExpectedTaskCount.load(std::memory_order_acquire);
		check(Count > 0);

		TSharedPtr<FAsyncMultiHandle> PinnedRoot = Root.Pin();
		
		if (!PinnedRoot) { return; }
		StaticCastSharedPtr<FTaskManager>(PinnedRoot)->ReserveTasks(Count);

		for (int i = 0; i < Count; i++)
		{
			PCGEX_MAKE_SHARED(Task, FSimpleCallbackTask, i)
			Launch(Task);
		}
	}

	void FTaskGroup::TriggerSimpleCallback(const int32 Index)
	{
		if (!IsAvailable()) { return; }

		SimpleCallbacks[Index]();
	}

	void FTaskGroup::ExecScopeIterations(const FScope& Scope, const bool bPrepareOnly) const
	{
		if (!IsAvailable()) { return; }

		if (OnSubLoopStartCallback) { OnSubLoopStartCallback(Scope); }

		if (bPrepareOnly) { return; }

		for (int i = Scope.Start; i < Scope.End; i++) { OnIterationCallback(i, Scope); }
	}

	void FSimpleCallbackTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		const TSharedPtr<FAsyncMultiHandle> PinnedParent = ParentHandle.Pin();
		if (!PinnedParent) { return; }

		StaticCastSharedPtr<FTaskGroup>(PinnedParent)->TriggerSimpleCallback(TaskIndex);
	}

	void FScopeIterationTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		const TSharedPtr<FAsyncMultiHandle> PinnedParent = ParentHandle.Pin();
		if (!PinnedParent) { return; }

		StaticCastSharedPtr<FTaskGroup>(PinnedParent)->ExecScopeIterations(Scope, bPrepareOnly);
	}

	void FDaisyChainScopeIterationTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		const TSharedPtr<FAsyncMultiHandle> PinnedParent = ParentHandle.Pin();
		if (!PinnedParent) { return; }

		const TSharedPtr<FTaskGroup> Group = StaticCastSharedPtr<FTaskGroup>(PinnedParent);
		if (!Group->IsAvailable()) { return; }

		const FScope& Scope = Group->GetLoopScope(TaskIndex);
		Group->ExecScopeIterations(Scope, bPrepareOnly);

		if (!Group->GetLoopScopes().IsValidIndex(Scope.GetNextScopeIndex())) { return; }

		PCGEX_MAKE_SHARED(Task, FDaisyChainScopeIterationTask, Scope.GetNextScopeIndex())
		Group->LaunchWithPreparation(Task, bPrepareOnly);
	}

	void FDeferredCallbackTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		Callback();
	}

	void FDeferredCallbackWithManagerTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		Callback(AsyncManager);
	}

	bool FDeferredCallbackHandle::Start()
	{
		if (FAsyncHandle::Start())
		{
			Callback();
			return true;
		}
		return false;
	}
}
