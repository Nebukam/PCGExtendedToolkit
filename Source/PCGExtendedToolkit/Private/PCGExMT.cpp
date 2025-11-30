// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "PCGExMT.h"

#include "Tasks/Task.h"
#include "PCGExHelpers.h"
#include "PCGExContext.h"
#include "PCGExGlobalSettings.h"
#include "PCGExSubSystem.h"
#include "Details/PCGExWaitMacros.h"
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
		case EPCGExAsyncPriority::Normal:
			Priority = UE::Tasks::ETaskPriority::Normal;
			break;
		case EPCGExAsyncPriority::High:
			Priority = UE::Tasks::ETaskPriority::High;
			break;
		case EPCGExAsyncPriority::BackgroundHigh:
			Priority = UE::Tasks::ETaskPriority::BackgroundHigh;
			break;
		case EPCGExAsyncPriority::BackgroundNormal:
			Priority = UE::Tasks::ETaskPriority::BackgroundNormal;
			break;
		case EPCGExAsyncPriority::BackgroundLow:
			Priority = UE::Tasks::ETaskPriority::BackgroundLow;
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

		for (int32 CurrentCount = 0; CurrentCount < MaxItems; CurrentCount += RangeSize)
		{
			OutSubRanges.Emplace(CurrentCount, FMath::Min(RangeSize, MaxItems - CurrentCount), OutSubRanges.Num());
		}

		return OutSubRanges.Num();
	}

	void AssertEmptyThread(const int32 MaxItems)
	{
		// This error can only be triggered from two places, and it's due to an edge-case that's setup-dependant
		// I have not been able to repro these edges cases (starting task groups with zero tasks, so they can't ever complete)
		// If you read this, please let me know what let you there so I can fix it!
		UE_LOG(LogPCGEx, Error, TEXT("StartRanges: MaxItems = %i // Graph will never finish execution! You can temporarily enable bAssertOnEmptyThread in PCGEx debug settings and hook a debugger to get a stack trace."), MaxItems);
		if (GetDefault<UPCGExGlobalSettings>()->bAssertOnEmptyThread) { ensure(false); }
	}

	FAsyncHandle::~FAsyncHandle()
	{
		Cancel(); // Safety first
	}

	bool FAsyncHandle::SetRoot(const TSharedPtr<FAsyncMultiHandle>& InRoot, const int32 InHandleIdx)
	{
		HandleIdx = InHandleIdx;
		Root = InRoot;
		return InRoot->IncrementPendingTasks();
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
		return CompareAndSetState(ExpectedState, EAsyncHandleState::Running);
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

	bool FAsyncMultiHandle::SetRoot(const TSharedPtr<FAsyncMultiHandle>& InRoot, const int32 InHandleIdx)
	{
		bForceSync = InRoot->bForceSync;
		return FAsyncHandle::SetRoot(InRoot, InHandleIdx);
	}

	bool FAsyncMultiHandle::IncrementPendingTasks()
	{
		if (!IsAvailable()) { return false; }

		PendingTaskCount.fetch_add(1, std::memory_order_release);
		HandleTaskStart();
		return true;
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
		: FAsyncMultiHandle(InForceSync, FName("ROOT")), Context(InContext), ContextHandle(InContext->GetOrCreateHandle())
	{
		WorkHandle = Context->GetWorkHandle();
	}

	FTaskManager::~FTaskManager()
	{
		Cancel();
	}

	bool FTaskManager::IsAvailable() const
	{
		return (ContextHandle.IsValid() && !IsCancelling() && !IsCancelled() && !IsResetting() && WorkHandle.IsValid());
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
			UE_LOG(LogPCGEx, Error, TEXT("Attempting to register async work while manager is unavailable. If there's a crash, it's probably because this happened."));
			return false;
		}

		if (GetState() == EAsyncHandleState::Running)
		{
			if (!Context->bIsPaused)
			{
				UE_LOG(LogPCGEx, Warning, TEXT("Task Manager started a task while it was already running, but the context was somehow unpaused?"));
			}

			return true;
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
			for (const TWeakPtr<FAsyncHandle>& WeakTask : Tasks) { if (const TSharedPtr<FAsyncHandle> Task = WeakTask.Pin()) { Task->Cancel(); } }
			Tasks.Empty();
		}

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExTaskManager::WaitingForRunningTasks);

			// Fail safe for tasks that cannot be cancelled mid-way
			// This will only be false in cases where lots of regen/cancel happen in the same frame.
			PCGEX_ASYNC_WAIT_CHKD(IsWaitingForRunningTasks())
		}

		Tokens.Empty();
		Groups.Empty();

		bIsCancelling.store(false, std::memory_order_release);

		if (!IsResetting())
		{
			// TODO : We should call end here if we're not resetting
		}

		return true;
	}

	void FTaskManager::Reset()
	{
		if (!IsAvailable()) { return; } // Don't reset if we're already cancelled

		bIsResetting.store(true, std::memory_order_release);

		Cancel(); // Cancel ongoing work first, just in case
		// TODO : We should call End() here instead of unpausing context afterward?
		// NOTE : Actually maybe not, End is triggering the OnComplete() callback.
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
		if (NewGroup->SetRoot(SharedThis(this), -1))
		{
			// So its state can be updated properly
			NewGroup->Start();
			return Groups.Add_GetRef(NewGroup);
		}
		return nullptr;
	}

	bool FTaskManager::TryRegisterHandle(const TSharedPtr<FAsyncHandle>& InHandle)
	{
		check(InHandle->HandleIdx == -1)

		if (!IsAvailable()) { return false; }

		int32 Idx = -1;
		{
			FWriteScopeLock WriteTasksLock(TasksLock);
			Idx = Tasks.Add(InHandle);
		}

		TSharedPtr<FTaskManager> LocalManager = SharedThis(this);
		if (InHandle->SetRoot(LocalManager, Idx))
		{
			InHandle->Start();
			return true;
		}

		return false;
	}

	TWeakPtr<FAsyncToken> FTaskManager::TryCreateToken(const FName& TokenName)
	{
		FWriteScopeLock WriteGroupsLock(TokensLock);

		if (!IsAvailable()) { return nullptr; }

		PCGEX_MAKE_SHARED(Token, FAsyncToken, SharedThis(this), TokenName)
		return Tokens.Add_GetRef(Token);
	}

	void FTaskManager::DeferredReset(FSimpleCallback&& Callback)
	{
		// Reset from outside then callback
		// Use this to chain/clear heavy workloads
		UE::Tasks::Launch(
				TEXT("ResetThen"),
				[PCGEX_ASYNC_THIS_CAPTURE, Callback]()
				{
					PCGEX_ASYNC_THIS
					This->Reset();
					Callback();
				},
				UE::Tasks::ETaskPriority::High
			);
	}

	void FTaskManager::DeferredResumeExecution(FSimpleCallback&& Callback) const
	{
		UE::Tasks::Launch(
				TEXT("ResetThen"),
				[CtxHandle = Context->GetOrCreateHandle(), Callback]()
				{
					PCGEX_SHARED_CONTEXT_VOID(CtxHandle)
					SharedContext.Get()->ResumeExecution();
					Callback();
				},
				UE::Tasks::ETaskPriority::High
			);
	}

	void FTaskManager::HandleTaskStart()
	{
		Start();
		FAsyncMultiHandle::HandleTaskStart();
	}

	void FTaskManager::End(const bool bIsCancellation)
	{
		// TODO : When is unpausing safe here?
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
		if (!InTask->SetRoot(LocalManager, Idx)) { return; }

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

	void FTaskGroup::StartIterations(const int32 MaxItems, const int32 ChunkSize, const bool bForceSingleThreaded)
	{
		if (!IsAvailable() || !OnIterationCallback) { return; }

		check(MaxItems > 0);

		const int32 SanitizedChunkSize = FMath::Max(1, ChunkSize);

		if (bForceSingleThreaded)
		{
			bForceSingleThreadeded = true;

			SetExpectedTaskCount(SubLoopScopes(Loops, MaxItems, SanitizedChunkSize));

			if (OnPrepareSubLoopsCallback) { OnPrepareSubLoopsCallback(Loops); }

			PCGEX_MAKE_SHARED(Task, FForceSingleThreadedScopeIterationTask, 0)
			LaunchWithPreparation(Task, false);
		}
		else
		{
			StartRanges<FScopeIterationTask>(MaxItems, SanitizedChunkSize, false);
		}
	}

	void FTaskGroup::StartSubLoops(const int32 MaxItems, const int32 ChunkSize, const bool bForceSingleThreaded)
	{
		if (!bForceSingleThreaded)
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

		bForceSingleThreadeded = true;

		if (OnPrepareSubLoopsCallback) { OnPrepareSubLoopsCallback(Loops); }

		PCGEX_MAKE_SHARED(Task, FForceSingleThreadedScopeIterationTask, 0)
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

		PCGEX_SCOPE_LOOP(i) { OnIterationCallback(i, Scope); }
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

	void FForceSingleThreadedScopeIterationTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		const TSharedPtr<FAsyncMultiHandle> PinnedParent = ParentHandle.Pin();
		if (!PinnedParent) { return; }

		const TSharedPtr<FTaskGroup> Group = StaticCastSharedPtr<FTaskGroup>(PinnedParent);
		if (!Group->IsAvailable()) { return; }

		const FScope& Scope = Group->GetLoopScope(TaskIndex);
		Group->ExecScopeIterations(Scope, bPrepareOnly);

		if (!Group->GetLoopScopes().IsValidIndex(Scope.GetNextScopeIndex())) { return; }

		PCGEX_MAKE_SHARED(Task, FForceSingleThreadedScopeIterationTask, Scope.GetNextScopeIndex())
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

	TSharedPtr<FDeferredCallbackHandle> DeferredCallback(FPCGExContext* InContext, FSimpleCallback&& InCallback)
	{
		TSharedPtr<FDeferredCallbackHandle> DeferredCallback = MakeShared<FDeferredCallbackHandle>();
		DeferredCallback->Callback = InCallback;

		TWeakPtr<FDeferredCallbackHandle> WeakTask = DeferredCallback;
		UE::Tasks::Launch(
				TEXT("DeferredCallback"),
				[WeakTask]()
				{
					const TSharedPtr<FDeferredCallbackHandle> Task = WeakTask.Pin();
					if (!Task.IsValid()) { return; }
					if (Task->Start()) { Task->Complete(); }
				},
				UE::Tasks::ETaskPriority::Default
			);

		return DeferredCallback;
	}

	void CancelDeferredCallback(const TSharedPtr<FDeferredCallbackHandle>& InCallback)
	{
		InCallback->Cancel();
		while (InCallback->GetState() != EAsyncHandleState::Ended)
		{
			// Hold off until ended
		}
	}

	FScopeLoopOnMainThread::FScopeLoopOnMainThread(const int32 NumIterations)
		: Scope(FScope(0, NumIterations, 0))
	{
	}


	FExecuteOnMainThread::FExecuteOnMainThread()
	{
	}

	bool FExecuteOnMainThread::Start()
	{
		if (!FAsyncHandle::Start()) { return false; }
		if (!CanRun()) { Complete(); }
		else { Schedule(); }
		return true;
	}

	void FExecuteOnMainThread::Schedule()
	{
		if (!CanRun())
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
		if (!CanRun())
		{
			Complete();
			return false;
		}

		FPCGExContext* InContext = nullptr;
		if (const TSharedPtr<FAsyncMultiHandle> PinnedRoot = Root.Pin())
		{
			TSharedPtr<FTaskManager> PinnedManager = StaticCastSharedPtr<FTaskManager>(PinnedRoot);
			if (PinnedManager->IsAvailable()) { InContext = PinnedManager->GetContext(); }
		}

		if (!InContext) { return true; }

		Complete();

		return true;
	}

	void FExecuteOnMainThread::End(bool bIsCancellation)
	{
		if (!bIsCancellation && OnCompleteCallback) { OnCompleteCallback(); }
		FAsyncHandle::End(bIsCancellation);
	}

	bool FExecuteOnMainThread::ShouldStop()
	{
		return FPlatformTime::Seconds() > EndTime;
	}

	bool FExecuteOnMainThread::CanRun()
	{
		return !IsCancelled() && GetState() == EAsyncHandleState::Running;
	}

	bool FScopeLoopOnMainThread::Start()
	{
		check(OnIterationCallback) // Why would you start it without a callback
		return FExecuteOnMainThread::Start();
	}

	bool FScopeLoopOnMainThread::Cancel()
	{
		if (FExecuteOnMainThread::Cancel()) { return true; }
		Complete();
		return false;
	}

	bool FScopeLoopOnMainThread::Execute()
	{
		if (!CanRun()) { return true; }

		FPCGExContext* InContext = nullptr;
		if (const TSharedPtr<FAsyncMultiHandle> PinnedRoot = Root.Pin())
		{
			TSharedPtr<FTaskManager> PinnedManager = StaticCastSharedPtr<FTaskManager>(PinnedRoot);
			if (PinnedManager->IsAvailable()) { InContext = PinnedManager->GetContext(); }
		}

		if (!InContext) { return true; }
		if (Scope.Start >= Scope.End) { return true; }

		PCGEX_SHARED_CONTEXT_RET(InContext->GetOrCreateHandle(), true)

		FPCGAsyncState AsyncState = SharedContext.Get()->AsyncState;

		if (ShouldStop()) { return false; }

		PCGEX_SCOPE_LOOP(Index)
		{
			OnIterationCallback(Index, Scope);

			if (ShouldStop())
			{
				Scope.Start = Index + 1;
				Scope.LoopIndex++;
				return Scope.Start >= Scope.End;
			}
		}

		return true;
	}
}
