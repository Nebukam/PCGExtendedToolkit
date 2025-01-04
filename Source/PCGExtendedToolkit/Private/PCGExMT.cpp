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

	void FAsyncHandle::SetRoot(const TSharedPtr<FAsyncMultiHandle>& InRoot)
	{
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
		{
			FReadScopeLock Lock(StateLock);
			if (bCancelled) { return false; }
			if (State != EAsyncHandleState::Idle) { return false; }
		}

		{
			FWriteScopeLock Lock(StateLock);
			if (bCancelled) { return false; }
			if (State != EAsyncHandleState::Idle) { return false; }
			State = EAsyncHandleState::Running;
			return true;
		}
	}

	void FAsyncHandle::Cancel()
	{
		{
			FReadScopeLock Lock(StateLock);
			if (bCancelled) { return; }
		}
		{
			FWriteScopeLock Lock(StateLock);
			if (bCancelled) { return; }

			bCancelled = true;

			// Only trigger end if Idle
			// If work has start, End() will be called.
			if (State == EAsyncHandleState::Idle) { EndInternal(); }
		}
	}

	bool FAsyncHandle::IsCancelled() const
	{
		FReadScopeLock Lock(StateLock);
		return bCancelled;
	}

	void FAsyncHandle::End()
	{
		{
			// End notifies hierarchy and as such doesn't care if handle is cancelled or not
			FWriteScopeLock Lock(StateLock);
			EndInternal();
		}
	}

	void FAsyncHandle::EndInternal()
	{
		if (State == EAsyncHandleState::Ended) { return; }
		State = EAsyncHandleState::Ended;

		if (const TSharedPtr<FAsyncMultiHandle> PinnedRoot = Root.Pin()) { PinnedRoot->IncrementCompletedTasks(); }
		if (const TSharedPtr<FAsyncMultiHandle> PinnedParent = ParentHandle.Pin()) { PinnedParent->IncrementCompletedTasks(); }
	}

	FAsyncMultiHandle::FAsyncMultiHandle(const bool InForceSync, const FName InName)
		: bForceSync(InForceSync), GroupName(InName)
	{
	}

	void FAsyncMultiHandle::SetRoot(const TSharedPtr<FAsyncMultiHandle>& InRoot)
	{
		bForceSync = InRoot->bForceSync;
		FAsyncHandle::SetRoot(InRoot);
	}

	void FAsyncMultiHandle::IncrementPendingTasks()
	{
		FWriteScopeLock Lock(GrowthLock);
		PendingTaskCount++;
		HandleTaskStart();
	}

	void FAsyncMultiHandle::IncrementCompletedTasks()
	{
		FWriteScopeLock Lock(GrowthLock);
		CompletedTaskCount++;
		HandleTaskCompletion();
	}

	bool FAsyncMultiHandle::IsAvailable() const
	{
		if (const TSharedPtr<FAsyncMultiHandle> PinnedRoot = Root.Pin()) { if (!PinnedRoot->IsAvailable()) { return false; } }
		else { return false; }

		{
			FReadScopeLock Lock(StateLock);
			return !(bCancelled || State == EAsyncHandleState::Ended);
		}
	}

	void FAsyncMultiHandle::HandleTaskStart()
	{
	}

	void FAsyncMultiHandle::HandleTaskCompletion()
	{
		if (PendingTaskCount == CompletedTaskCount)
		{
			EndInternal();
		}
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

	void FAsyncMultiHandle::EndInternal()
	{
		if (State == EAsyncHandleState::Ended) { return; }
		// Complete callback before notifying hierarchy
		if (!IsCancelled() && OnCompleteCallback) { OnCompleteCallback(); }
		FAsyncHandle::EndInternal();
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
		FWriteScopeLock WriteScopeLock(ReleaseLock);
		if (const TSharedPtr<FAsyncMultiHandle> PinnedHandle = Handle.Pin())
		{
			PinnedHandle->IncrementCompletedTasks();
			Handle.Reset();
		}
	}

	FTaskManager::FTaskManager(FPCGExContext* InContext, const bool InForceSync)
		: FAsyncMultiHandle(InForceSync, FName("ROOT")), Context(InContext)
	{
		PCGEX_LOG_CTR(FTaskManager)
		Lifeline = Context->Lifeline;
	}

	FTaskManager::~FTaskManager()
	{
		PCGEX_LOG_DTR(FTaskManager)
		Cancel();
	}

	bool FTaskManager::IsAvailable() const
	{
		FReadScopeLock Lock(ManagerStateLock);
		return (!bCancelling && !bCancelled && !bResetting && Lifeline.IsValid());
	}

	bool FTaskManager::IsWaitingForRunningTasks() const
	{
		FReadScopeLock Lock(StateLock);
		return !bForceSync && State == EAsyncHandleState::Running;
	}

	bool FTaskManager::Start()
	{
		// Called whenever a handle registers running work to root
		// Normally nothing would trigger here
		check(IsAvailable())

		Context->PauseContext();

		{
			FWriteScopeLock Lock(StateLock);
			State = EAsyncHandleState::Running;
		}

		return true;
	}

	void FTaskManager::Cancel()
	{
		{
			FWriteScopeLock WriteScopeLock(ManagerStateLock);
			if (bCancelling || bCancelled) { return; }
			bCancelling = true;
		}

		{
			FWriteScopeLock WriteTokenLock(TokensLock);
			FWriteScopeLock WriteTasksLock(TasksLock);
			FWriteScopeLock WriteGroupsLock(GroupsLock);

			Tokens.Empty(); // Revoke all tokens

			// Cancel groups & tasks
			for (const TSharedPtr<FTask>& Task : Tasks) { Task->Cancel(); }
			for (const TSharedPtr<FTaskGroup>& Group : Groups) { Group->Cancel(); }
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
		Tasks.Empty();

		bCancelled = true;
		bCancelling = false;
	}

	void FTaskManager::Reset()
	{
		if (!IsAvailable())
		{
			// No reset if manager was stopped before
			// Since this means it was stopped from outside this loop
			// Hence by destructor or aborted execution
			return;
		}

		bResetting = true;

		// Stop & flush so no new task may be created
		Cancel();

		// Ensure we can keep on taking tasks
		bCancelled = false;
		PendingTaskCount = 0;
		CompletedTaskCount = 0;

		bResetting = false;

		// Safety unpause
		Context->UnpauseContext();
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
		NewGroup->SetRoot(SharedThis(this));

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

	void FTaskManager::EndInternal()
	{
		if (State == EAsyncHandleState::Ended) { return; }
		FAsyncMultiHandle::EndInternal();
		Context->UnpauseContext();
	}

	void FTaskManager::StartBackgroundTask(const TSharedPtr<FTask>& InTask)
	{
		if (!IsAvailable()) { return; }

		{
			FWriteScopeLock WriteTasksLock(TasksLock);
			Tasks.Add(InTask);
		}

		TSharedPtr<FTaskManager> LocalManager = SharedThis(this);
		InTask->SetRoot(LocalManager);

		PCGEX_SHARED_THIS_DECL
		UE::Tasks::Launch(
				*InTask->HandleId(),
				[
					WeakManager = TWeakPtr<FTaskManager>(LocalManager),
					WeakTask = TWeakPtr<FTask>(InTask)]()
				{
					const TSharedPtr<FTaskManager> Manager = WeakManager.Pin();
					const TSharedPtr<FTask> Task = WeakTask.Pin();

					if (!Task || !Manager || !Manager->IsAvailable()) { return; }

					if (Task->Start())
					{
						Task->ExecuteTask(Manager);
						Task->End();
					}
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

			ExpectedTaskCount = SubLoopScopes(Loops, MaxItems, SanitizedChunkSize);

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
		ExpectedTaskCount = SubLoopScopes(Loops, MaxItems, FMath::Max(1, ChunkSize));
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
		ExpectedTaskCount = SimpleCallbacks.Num();
		check(ExpectedTaskCount > 0);

		TSharedPtr<FAsyncMultiHandle> PinnedRoot = Root.Pin();
		if (!PinnedRoot) { return; }
		StaticCastSharedPtr<FTaskManager>(PinnedRoot)->ReserveTasks(ExpectedTaskCount);

		for (int i = 0; i < ExpectedTaskCount; i++)
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
}
