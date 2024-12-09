// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "PCGExMT.h"
#include "Tasks/Task.h"


namespace PCGExMT
{
	FTaskManager::~FTaskManager()
	{
		PCGEX_LOG_DTR(FTaskManager)
		Reset(true);
	}

	void FTaskManager::GrowNumStarted()
	{
		Context->PauseContext();
		FPlatformAtomics::InterlockedExchange(&WorkComplete, 0);
		FPlatformAtomics::InterlockedAdd(&NumStarted, 1);
	}

	void FTaskManager::GrowNumCompleted()
	{
		FPlatformAtomics::InterlockedAdd(&NumCompleted, 1);
		if (NumCompleted == NumStarted) { ScheduleCompletion(); }
	}

	TSharedPtr<FTaskGroup> FTaskManager::TryCreateGroup(const FName& GroupName)
	{
		if (!IsAvailable()) { return nullptr; }

		{
			FWriteScopeLock WriteLock(GroupLock);
			return Groups.Add_GetRef(MakeShared<FTaskGroup>(SharedThis(this), GroupName));
		}
	}

	void FTaskManager::StartBackgroundTask(const TSharedPtr<FPCGExTask>& InTask, const TSharedPtr<FTaskGroup>& InGroup)
	{
		if (!IsAvailable()) { return; }

		{
			FWriteScopeLock WriteLock(ManagerLock);
			GrowNumStarted();
			QueuedTasks.Add(InTask.ToSharedRef());
		}

		UE::Tasks::Launch(
				*InTask->GetTaskName(),
				[
					WeakTask = TWeakPtr<FPCGExTask>(InTask),
					WeakManager = TWeakPtr<FTaskManager>(SharedThis(this)),
					WeakGroup = TWeakPtr<FTaskGroup>(InGroup)]()
				{
					const TSharedPtr<FPCGExTask> Task = WeakTask.Pin();
					if (!Task || Task->IsCanceled()) { return; }

					const TSharedPtr<FTaskManager> Manager = WeakManager.Pin();
					if (!Manager || !Manager->IsAvailable()) { return; }

					if (const TSharedPtr<FTaskGroup> Group = WeakGroup.Pin())
					{
						if (Group->IsAvailable()) { Task->ExecuteTask(Manager, Group); }
						Group->GrowNumCompleted();
					}
					else
					{
						Task->ExecuteTask(Manager, nullptr);
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
		return ForceSync || WorkComplete > 0;
	}

	void FTaskManager::Reset(const bool bHoldStop)
	{
		if (Stopped) { return; }

		FPlatformAtomics::InterlockedExchange(&CompletionScheduled, 0);
		FPlatformAtomics::InterlockedExchange(&WorkComplete, 0);
		FPlatformAtomics::InterlockedExchange(&Stopped, 1);

		FWriteScopeLock WriteLock(ManagerLock);

		for (const TSharedPtr<FTaskGroup>& Group : Groups) { FPlatformAtomics::InterlockedExchange(&Group->bAvailable, 0); }
		for (const TSharedPtr<FPCGExTask>& Task : QueuedTasks) { Task->Cancel(); }

		QueuedTasks.Empty();
		Groups.Empty();

		if (!bHoldStop) { FPlatformAtomics::InterlockedExchange(&Stopped, 0); }

		NumStarted = 0;
		NumCompleted = 0;

		Context->UnpauseContext();
	}

	void FTaskManager::ScheduleCompletion()
	{
		if (CompletionScheduled) { return; }

		FPlatformAtomics::InterlockedExchange(&CompletionScheduled, 1);
		TryComplete();

		//const TSharedPtr<FTaskManager> SharedSelf = SharedThis(this);
		//TWeakPtr<FTaskManager> AsyncThis = SharedSelf;

		//AsyncTask(
		//	ENamedThreads::BackgroundThreadPriority, [AsyncThis]()
		//	{
		//		const TSharedPtr<FTaskManager> Manager = AsyncThis.Pin();
		//		if (!Manager || !Manager->IsAvailable()) { return; }
		//		Manager->TryComplete();
		//	});
	}

	void FTaskManager::TryComplete()
	{
		if (!CompletionScheduled) { return; }
		FPlatformAtomics::InterlockedExchange(&CompletionScheduled, 0);

		if (Stopped)
		{
			Context->UnpauseContext(); // Ensure context is unpaused
			return;
		}

		if (NumCompleted == NumStarted)
		{
			FPlatformAtomics::InterlockedExchange(&WorkComplete, 1);
			Context->UnpauseContext(); // Unpause context
		}
	}

	bool FTaskGroup::IsAvailable() const
	{
		return bAvailable && Manager->IsAvailable();
	}

	void FTaskGroup::StartIterations(const int32 MaxItems, const int32 ChunkSize, const bool bInlined)
	{
		if (!IsAvailable() || !OnIterationCallback) { return; }

		check(MaxItems > 0);

		const int32 SanitizedChunkSize = FMath::Max(1, ChunkSize);

		if (bInlined)
		{
			TArray<FScope> Loops;
			GrowNumStarted(SubLoopScopes(Loops, MaxItems, SanitizedChunkSize));
			if (OnPrepareSubLoopsCallback) { OnPrepareSubLoopsCallback(Loops); }

			PCGEX_MAKE_SHARED(Task, FInlinedScopeIterationTask, 0)
			InternalStartInlineRange(Task, false, Loops);
		}
		else
		{
			StartRanges<FScopeIterationTask>(MaxItems, SanitizedChunkSize, false);
		}
	}

	void FTaskGroup::StartSubLoops(const int32 MaxItems, const int32 ChunkSize, const bool bInline)
	{
		if (!IsAvailable()) { return; }

		check(MaxItems > 0);
		const int32 SanitizedChunkSize = FMath::Max(1, ChunkSize);

		if (bInline)
		{
			TArray<FScope> Loops;
			GrowNumStarted(SubLoopScopes(Loops, MaxItems, SanitizedChunkSize));
			if (OnPrepareSubLoopsCallback) { OnPrepareSubLoopsCallback(Loops); }

			PCGEX_MAKE_SHARED(Task, FInlinedScopeIterationTask, 0)
			InternalStartInlineRange(Task, true, Loops);
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
			InternalStart(Task, false);
		}
	}

	void FTaskGroup::GrowNumStarted(const int32 InNumStarted)
	{
		FWriteScopeLock WriteScopeLock(GroupLock);
		FPlatformAtomics::InterlockedAdd(&NumStarted, InNumStarted);
	}

	void FTaskGroup::GrowNumCompleted()
	{
		if (!IsAvailable()) { return; }

		{
			FWriteScopeLock WriteScopeLock(GroupLock);
			FPlatformAtomics::InterlockedAdd(&NumCompleted, 1);
			if (NumCompleted == NumStarted)
			{
				NumCompleted = 0;
				NumStarted = 0;
				if (OnCompleteCallback) { OnCompleteCallback(); }
				Manager->GrowNumCompleted();
			}
		}
	}

	void FTaskGroup::PrepareRangeIteration(const FScope& Scope) const
	{
		if (!IsAvailable()) { return; }
		if (OnSubLoopStartCallback) { OnSubLoopStartCallback(Scope); }
	}

	void FTaskGroup::DoRangeIteration(const FScope& Scope, const bool bPrepareOnly) const
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

	void FInlinedScopeIterationTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager, const TSharedPtr<FTaskGroup>& InGroup)
	{
		ON_SCOPE_EXIT { Loops.Empty(); };

		if (!InGroup) { return; }

		const FScope& Scope = Loops[TaskIndex];
		InGroup->DoRangeIteration(Scope, bPrepareOnly);

		if (!Loops.IsValidIndex(Scope.GetNextScopeIndex())) { return; }

		PCGEX_MAKE_SHARED(Task, FInlinedScopeIterationTask, Scope.GetNextScopeIndex())
		InGroup->InternalStartInlineRange(Task, bPrepareOnly, Loops);
		Loops.Empty();
	}
}
