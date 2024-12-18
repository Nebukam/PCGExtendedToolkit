// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "PCGExMT.h"


#include "Tasks/Task.h"


namespace PCGExMT
{
	FTaskManager::FTaskManager(FPCGExContext* InContext)
		: Context(InContext)
	{
		PCGEX_LOG_CTR(FTaskManager)
		Lifecycle = Context->Lifecycle;
	}

	FTaskManager::~FTaskManager()
	{
		PCGEX_LOG_DTR(FTaskManager)
		Stop();
	}

	void FTaskManager::GrowNumStarted()
	{
		Context->PauseContext();
		FPlatformAtomics::InterlockedIncrement(&NumStarted);
		bWorkComplete = false;
	}

	void FTaskManager::GrowNumCompleted()
	{
		FPlatformAtomics::InterlockedIncrement(&NumCompleted);
		if (NumCompleted == NumStarted) { Complete(); }
	}

	TSharedPtr<FTaskGroup> FTaskManager::TryCreateGroup(const FName& GroupName)
	{
		if (!IsAvailable()) { return nullptr; }

		GrowNumStarted();

		{
			FWriteScopeLock WriteGroupsLock(GroupsLock);
			return Groups.Add_GetRef(MakeShared<FTaskGroup>(SharedThis(this), GroupName));
		}
	}

	void FTaskManager::StartBackgroundTask(const TSharedPtr<FPCGExTask>& InTask, const TSharedPtr<FTaskGroup>& InGroup)
	{
		if (!IsAvailable()) { return; }

		GrowNumStarted();

		{
			FWriteScopeLock WriteTasksLock(TasksLock);
			Tasks.Add(InTask);
		}

		InTask->ParentGroup = InGroup;

		PCGEX_SHARED_THIS_DECL
		UE::Tasks::Launch(
				*InTask->GetTaskName(),
				[
					WeakTask = TWeakPtr<FPCGExTask>(InTask),
					WeakManager = TWeakPtr<FTaskManager>(ThisPtr)]()
				{
					const TSharedPtr<FTaskManager> Manager = WeakManager.Pin();
					const TSharedPtr<FPCGExTask> Task = WeakTask.Pin();

					if (!Manager || !Manager->IsAvailable() || !Task || Task->IsCanceled()) { return; }

					const TSharedPtr<FTaskGroup> Group = Task->ParentGroup.Pin();

					if (Task->Start())
					{
						Task->ExecuteTask(Manager);
						Task->End(Manager);
					}
				},
				WorkPriority
			);
	}

	void FTaskManager::StartSynchronousTask(const TSharedPtr<FPCGExTask>& InTask, const TSharedPtr<FTaskGroup>& InGroup)
	{
		if (!IsAvailable()) { return; }

		GrowNumStarted();

		PCGEX_SHARED_THIS_DECL

		InTask->ParentGroup = InGroup;

		InTask->Start();
		InTask->ExecuteTask(ThisPtr);
		InTask->End(ThisPtr);
	}

	bool FTaskManager::IsWorkComplete() const
	{
		return bForceSync || bWorkComplete;
	}

	void FTaskManager::Stop()
	{
		if (bStopping || bStopped) { return; }

		bStopping = true;

		{
			FWriteScopeLock WriteTasksLock(TasksLock);
			FWriteScopeLock WriteGroupsLock(GroupsLock);

			PCGEX_SHARED_THIS_DECL

			// Cancel groups & tasks
			for (const TSharedPtr<FPCGExTask>& Task : Tasks) { Task->Cancel(ThisPtr); }
			for (const TSharedPtr<FTaskGroup>& Group : Groups) { Group->Cancel(); }
		}

		// Fail safe for tasks that cannot be cancelled mid-way
		// This will only be false in cases where lots of regen/cancel happen in the same frame.
		while (!IsWorkComplete())
		{
			// (；′⌒`)
		}

		{
			FWriteScopeLock WriteGroupsLock(GroupsLock);
			FWriteScopeLock WriteTasksLock(TasksLock);

			Groups.Empty();
			Tasks.Empty();
		}

		bStopped = true;
		bStopping = false;
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

		// Stop so no new task may be created
		Stop();

		// Ensure we can keep on taking tasks
		bStopped = false;
		NumStarted = 0;
		NumCompleted = 0;

		bWorkComplete = true;
		bResetting = false;

		// Unpause context just in case
		Context->UnpauseContext();
	}

	void FTaskManager::Complete()
	{
		if (bWorkComplete) { return; }

		bWorkComplete = true;

		// Unpause context
		Context->UnpauseContext();
	}

	void FTaskGroup::End()
	{
		if (bEnded) { return; }
		bEnded = true;

		if (!IsCanceled()) { if (OnCompleteCallback) { OnCompleteCallback(); } }

		if (const TSharedPtr<FTaskGroup> Group = ParentGroup.Pin()) { Group->GrowNumCompleted(); }
		Manager->GrowNumCompleted();
	}

	void FTaskGroup::Cancel()
	{
		bCanceled = true;
		End(); // Ensure we close the task in the manager if we have daisy-chained tasks
	}

	bool FTaskGroup::IsAvailable() const
	{
		return !IsCanceled() && Manager->IsAvailable();
	}

	void FTaskGroup::SetParentTaskGroup(const TSharedPtr<FTaskGroup>& InParentGroup)
	{
		if (!InParentGroup) { return; }
		check(ParentGroup == nullptr)
		ParentGroup = InParentGroup;
		InParentGroup->GrowNumStarted();
	}

	void FTaskGroup::StartIterations(const int32 MaxItems, const int32 ChunkSize, const bool bDaisyChain)
	{
		if (!IsAvailable() || !OnIterationCallback) { return; }

		check(MaxItems > 0);

		const int32 SanitizedChunkSize = FMath::Max(1, ChunkSize);

		if (bDaisyChain)
		{
			bDaisyChained = true;

			GrowNumStarted(SubLoopScopes(Loops, MaxItems, SanitizedChunkSize));
			if (OnPrepareSubLoopsCallback) { OnPrepareSubLoopsCallback(Loops); }

			PCGEX_MAKE_SHARED(Task, FDaisyChainScopeIterationTask, 0)
			LaunchDaisyChainScope(Task, false);
		}
		else
		{
			StartRanges<FScopeIterationTask>(MaxItems, SanitizedChunkSize, false);
		}
	}

	void FTaskGroup::StartSubLoops(const int32 MaxItems, const int32 ChunkSize, const bool bDaisyChain)
	{
		if (!IsAvailable()) { return; }

		check(MaxItems > 0);
		const int32 SanitizedChunkSize = FMath::Max(1, ChunkSize);

		if (bDaisyChain)
		{
			bDaisyChained = true;

			GrowNumStarted(SubLoopScopes(Loops, MaxItems, SanitizedChunkSize));
			if (OnPrepareSubLoopsCallback) { OnPrepareSubLoopsCallback(Loops); }

			PCGEX_MAKE_SHARED(Task, FDaisyChainScopeIterationTask, 0)
			LaunchDaisyChainScope(Task, true);
		}
		else
		{
			StartRanges<FScopeIterationTask>(MaxItems, SanitizedChunkSize, true);
		}
	}

	void FTaskGroup::AddSimpleCallback(SimpleCallback&& InCallback)
	{
		SimpleCallbacks.Add(InCallback);
	}

	void FTaskGroup::StartSimpleCallbacks()
	{
		const int32 NumSimpleCallbacks = SimpleCallbacks.Num();

		check(NumSimpleCallbacks > 0);

		for (int i = 0; i < NumSimpleCallbacks; i++)
		{
			PCGEX_MAKE_SHARED(Task, FSimpleCallbackTask, i)
			Launch(Task);
		}
	}

	void FTaskGroup::GrowNumStarted(const int32 InNumStarted)
	{
		FPlatformAtomics::InterlockedAdd(&NumStarted, InNumStarted);
	}

	void FTaskGroup::GrowNumCompleted()
	{
		FPlatformAtomics::InterlockedIncrement(&NumCompleted);
		if (NumStarted == NumCompleted) { End(); }
	}

	void FTaskGroup::PrepareScopeIteration(const FScope& Scope) const
	{
		if (!IsAvailable()) { return; }
		if (OnSubLoopStartCallback) { OnSubLoopStartCallback(Scope); }
	}

	void FTaskGroup::ExecScopeIterations(const FScope& Scope, const bool bPrepareOnly) const
	{
		if (!IsAvailable()) { return; }

		if (OnSubLoopStartCallback) { OnSubLoopStartCallback(Scope); }

		if (bPrepareOnly) { return; }

		for (int i = Scope.Start; i < Scope.End; i++) { OnIterationCallback(i, Scope); }
	}

	bool FPCGExTask::Start()
	{
		if (IsCanceled()) { return false; }

		{
			FWriteScopeLock WriteScopeLock(StateLock);
			check(!bWorkStarted)
			bWorkStarted = true;
		}

		return true;
	}

	void FPCGExTask::End(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		FWriteScopeLock WriteScopeLock(StateLock);

		if (bEnded) { return; }

		bWorkStarted = false;
		bEnded = true;

		if (const TSharedPtr<FTaskGroup>& Group = ParentGroup.Pin()) { Group->GrowNumCompleted(); }
		AsyncManager->GrowNumCompleted();
	}

	bool FPCGExTask::Cancel(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		{
			FWriteScopeLock WriteScopeLock(StateLock);

			// Return false if work is started & not completed yet.
			if (bWorkStarted) { return false; }
			if (bCanceled || bEnded) { return true; }

			bCanceled = true;
		}

		End(AsyncManager);

		return true;
	}

	void FDaisyChainScopeIterationTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		const TSharedPtr<FTaskGroup> Group = ParentGroup.Pin();
		if (!Group) { return; }

		const FScope& Scope = Group->GetLoopScope(TaskIndex);
		Group->ExecScopeIterations(Scope, bPrepareOnly);

		if (!Group->GetLoopScopes().IsValidIndex(Scope.GetNextScopeIndex())) { return; }

		PCGEX_MAKE_SHARED(Task, FDaisyChainScopeIterationTask, Scope.GetNextScopeIndex())
		Group->LaunchDaisyChainScope(Task, bPrepareOnly);
	}
}
