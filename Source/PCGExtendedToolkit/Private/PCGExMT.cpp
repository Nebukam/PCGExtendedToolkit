// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "PCGExMT.h"
#include "Tasks/Task.h"


namespace PCGExMT
{
	FTaskManager::~FTaskManager()
	{
		PCGEX_LOG_DTR(FTaskManager)
		Stop();
	}

	void FTaskManager::GrowNumStarted()
	{
		Context->PauseContext();
		bWorkComplete = false;
		FPlatformAtomics::InterlockedIncrement(&NumStarted);
	}

	void FTaskManager::GrowNumCompleted()
	{
		FPlatformAtomics::InterlockedIncrement(&NumCompleted);
		if (NumCompleted == NumStarted)
		{
			bWorkComplete = true;
			Context->UnpauseContext(); // Unpause context
		}
	}

	TSharedPtr<FTaskGroup> FTaskManager::TryCreateGroup(const FName& GroupName)
	{
		if (!IsAvailable()) { return nullptr; }

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


		PCGEX_SHARED_THIS_DECL
		UE::Tasks::Launch(
				*InTask->GetTaskName(),
				[
					WeakTask = TWeakPtr<FPCGExTask>(InTask),
					WeakManager = TWeakPtr<FTaskManager>(ThisPtr),
					WeakGroup = TWeakPtr<FTaskGroup>(InGroup)]()
				{
					const TSharedPtr<FTaskManager> Manager = WeakManager.Pin();
					const TSharedPtr<FPCGExTask> Task = WeakTask.Pin();
					const TSharedPtr<FTaskGroup> Group = WeakGroup.Pin();

					// Manager has been destroyed, exit early
					if (!Manager) { return; }

					// Manager is unavailable or task is cancelled
					if (!Manager->IsAvailable() || (Task && Task->IsCanceled()))
					{
						// Advance completion without execution to ensure proper flow
						// Manager may be unavailable because it was stopped
						if (Group) { Group->GrowNumCompleted(); }
						Manager->GrowNumCompleted();
						return;
					}

					// Task has been destroyed, exit early
					if (!Task) { return; }

					if (Group)
					{
						if (Group->IsAvailable())
						{
							Task->ExecuteTask(Manager, Group);
							Task->Complete();
						}
						Group->GrowNumCompleted();
					}
					else
					{
						Task->ExecuteTask(Manager, nullptr);
						Task->Complete();
					}

					Manager->GrowNumCompleted();
				},
				WorkPriority
			);
	}

	void FTaskManager::StartSynchronousTask(const TSharedPtr<FPCGExTask>& InTask, const TSharedPtr<FTaskGroup>& InGroup)
	{
		if (!IsAvailable()) { return; }

		GrowNumStarted();
		if (InGroup)
		{
			if (InGroup->IsAvailable()) { InTask->ExecuteTask(SharedThis(this), InGroup); }
			InGroup->GrowNumCompleted();
		}
		else
		{
			InTask->ExecuteTask(SharedThis(this), InGroup);
		}
		GrowNumCompleted();
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

			// Cancel groups & tasks
			for (const TSharedPtr<FTaskGroup>& Group : Groups) { Group->Cancel(); }
			for (const TSharedPtr<FPCGExTask>& Task : Tasks) { Task->Cancel(); }
		}

		while (!IsWorkComplete())
		{
			// Fail safe for tasks that cannot be cancelled mid-way
		}

		{
			FWriteScopeLock WriteTasksLock(TasksLock);
			FWriteScopeLock WriteGroupsLock(GroupsLock);

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

	void FTaskGroup::End()
	{
		if (bEnded) { return; }
		bEnded = true;

		if (!IsCanceled())
		{
			if (OnCompleteCallback) { OnCompleteCallback(); }
			if (ParentTaskGroup) { ParentTaskGroup->GrowNumCompleted(); }
		}

		ParentTaskGroup.Reset();
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
		check(ParentTaskGroup == nullptr)
		ParentTaskGroup = InParentGroup;
		ParentTaskGroup->GrowNumStarted();
	}

	void FTaskGroup::StartIterations(const int32 MaxItems, const int32 ChunkSize, const bool bDaisyChain)
	{
		if (!IsAvailable() || !OnIterationCallback) { return; }

		check(MaxItems > 0);

		const int32 SanitizedChunkSize = FMath::Max(1, ChunkSize);

		if (bDaisyChain)
		{
			bDaisyChained = true;

			TArray<FScope> Loops;
			GrowNumStarted(SubLoopScopes(Loops, MaxItems, SanitizedChunkSize));
			if (OnPrepareSubLoopsCallback) { OnPrepareSubLoopsCallback(Loops); }

			PCGEX_MAKE_SHARED(Task, FDaisyChainScopeIterationTask, 0)
			LaunchDaisyChainScope(Task, false, Loops);
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

			TArray<FScope> Loops;
			GrowNumStarted(SubLoopScopes(Loops, MaxItems, SanitizedChunkSize));
			if (OnPrepareSubLoopsCallback) { OnPrepareSubLoopsCallback(Loops); }

			PCGEX_MAKE_SHARED(Task, FDaisyChainScopeIterationTask, 0)
			LaunchDaisyChainScope(Task, true, Loops);
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

		GrowNumStarted(NumSimpleCallbacks);
		for (int i = 0; i < NumSimpleCallbacks; i++)
		{
			PCGEX_MAKE_SHARED(Task, FSimpleCallbackTask, i)
			Launch(Task, false);
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

	FString FPCGExTask::GetTaskName() const
	{
#if WITH_EDITOR
		return FString(typeid(this).name());
#else
		return TEXT("");
#endif
	}

	void FPCGExTask::Complete()
	{
		bCompleted = true;
	}

	void FPCGExTask::Cancel()
	{
		if (bCanceled || bCompleted) { return; }
		bCanceled = true;
	}

	void FDaisyChainScopeIterationTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager, const TSharedPtr<FTaskGroup>& InGroup)
	{
		ON_SCOPE_EXIT { Loops.Empty(); };

		if (!InGroup) { return; }

		const FScope& Scope = Loops[TaskIndex];
		InGroup->ExecScopeIterations(Scope, bPrepareOnly);

		if (!Loops.IsValidIndex(Scope.GetNextScopeIndex())) { return; }

		PCGEX_MAKE_SHARED(Task, FDaisyChainScopeIterationTask, Scope.GetNextScopeIndex())
		InGroup->LaunchDaisyChainScope(Task, bPrepareOnly, Loops);
		Loops.Empty();
	}
}
