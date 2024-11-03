// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "PCGExMT.h"

///*BUILD_TOOL_BUG_55_TOGGLE*/#include "CoreMinimal.h"

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

		for (const TSharedPtr<FTaskGroup> Group : Groups) { FPlatformAtomics::InterlockedExchange(&Group->bAvailable, 0); }

		for (FAsyncTaskBase* Task : QueuedTasks)
		{
			if (Task && !Task->Cancel()) { Task->EnsureCompletion(false); }
			delete Task;
		}

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
		//TWeakPtr<FTaskManager> WeakThisPtr = SharedSelf;

		//AsyncTask(
		//	ENamedThreads::BackgroundThreadPriority, [WeakThisPtr]()
		//	{
		//		const TSharedPtr<FTaskManager> Manager = WeakThisPtr.Pin();
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

	void FTaskGroup::StartIterations(const int32 MaxItems, const int32 ChunkSize, const bool bInlined, const bool bExecuteSmallSynchronously)
	{
		if (!IsAvailable() || !OnIterationCallback) { return; }

		check(MaxItems > 0);

		const int32 SanitizedChunkSize = FMath::Max(1, ChunkSize);

		if (MaxItems <= SanitizedChunkSize && bExecuteSmallSynchronously)
		{
			GrowNumStarted();
			if (OnIterationRangePrepareCallback) { OnIterationRangePrepareCallback({PCGEx::H64(0, MaxItems)}); }
			DoRangeIteration(0, MaxItems, 0);
			GrowNumCompleted();
			return;
		}

		if (bInlined)
		{
			TArray<uint64> Loops;
			GrowNumStarted(SubRanges(Loops, MaxItems, SanitizedChunkSize));
			if (OnIterationRangePrepareCallback) { OnIterationRangePrepareCallback(Loops); }
			InternalStartInlineRange<FGroupRangeInlineIterationTask>(0, Loops);
		}
		else
		{
			StartRanges<FGroupRangeIterationTask>(MaxItems, SanitizedChunkSize, nullptr);
		}
	}

	void FTaskGroup::StartRangePrepareOnly(const int32 MaxItems, const int32 ChunkSize, const bool bInline)
	{
		if (!IsAvailable()) { return; }

		check(MaxItems > 0);

		const int32 SanitizedChunkSize = FMath::Max(1, ChunkSize);

		if (bInline)
		{
			TArray<uint64> Loops;
			GrowNumStarted(SubRanges(Loops, MaxItems, SanitizedChunkSize));
			if (OnIterationRangePrepareCallback) { OnIterationRangePrepareCallback(Loops); }
			InternalStartInlineRange<FGroupPrepareRangeInlineTask>(0, Loops);
		}
		else { StartRanges<FGroupPrepareRangeTask>(MaxItems, SanitizedChunkSize, nullptr); }
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
		for (int i = 0; i < NumSimpleCallbacks; i++) { InternalStart<FSimpleCallbackTask>(false, i, nullptr); }
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

	void FTaskGroup::PrepareRangeIteration(const int32 StartIndex, const int32 Count, const int32 LoopIdx) const
	{
		if (!IsAvailable()) { return; }
		if (OnIterationRangeStartCallback) { OnIterationRangeStartCallback(StartIndex, Count, LoopIdx); }
	}

	void FTaskGroup::DoRangeIteration(const int32 StartIndex, const int32 Count, const int32 LoopIdx) const
	{
		if (!IsAvailable()) { return; }
		PrepareRangeIteration(StartIndex, Count, LoopIdx);
		for (int i = 0; i < Count; i++) { OnIterationCallback(StartIndex + i, Count, LoopIdx); }
	}

	void FPCGExTask::DoWork()
	{
		if (bWorkDone) { return; }
		bWorkDone = true;

		if (const TSharedPtr<FTaskManager> Manager = ManagerPtr.Pin())
		{
			if (!Manager->IsAvailable()) { return; }

			if (const TSharedPtr<FTaskGroup> Group = GroupPtr.Pin())
			{
				if (Group->IsAvailable()) { ExecuteTask(Manager); }
				Group->GrowNumCompleted();
			}
			else
			{
				ExecuteTask(Manager);
			}

			Manager->GrowNumCompleted();
		}
	}

	bool FSimpleCallbackTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		if (const TSharedPtr<FTaskGroup> Group = GroupPtr.Pin()) { Group->SimpleCallbacks[TaskIndex](); }
		return true;
	}

	bool FGroupRangeIterationTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		if (const TSharedPtr<FTaskGroup> Group = GroupPtr.Pin())
		{
			Group->DoRangeIteration(PCGEx::H64A(Scope), PCGEx::H64B(Scope), TaskIndex);
		}
		return true;
	}

	bool FGroupPrepareRangeTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		if (const TSharedPtr<FTaskGroup> Group = GroupPtr.Pin())
		{
			Group->PrepareRangeIteration(PCGEx::H64A(Scope), PCGEx::H64B(Scope), TaskIndex);
		}
		return true;
	}

	bool FGroupPrepareRangeInlineTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		ON_SCOPE_EXIT { Loops.Empty(); };
		
		if (const TSharedPtr<FTaskGroup> Group = GroupPtr.Pin())
		{
			uint32 StartIndex;
			uint32 Count;
			PCGEx::H64(Loops[TaskIndex], StartIndex, Count);

			Group->PrepareRangeIteration(StartIndex, Count, TaskIndex);

			if (!Loops.IsValidIndex(TaskIndex + 1)) { return false; }

			Group->InternalStartInlineRange<FGroupPrepareRangeInlineTask>(TaskIndex + 1, Loops);
		}
		return true;
	}

	bool FGroupRangeInlineIterationTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		ON_SCOPE_EXIT { Loops.Empty(); };
		
		if (const TSharedPtr<FTaskGroup> Group = GroupPtr.Pin())
		{
			uint32 StartIndex;
			uint32 Count;
			PCGEx::H64(Loops[TaskIndex], StartIndex, Count);

			Group->DoRangeIteration(StartIndex, Count, TaskIndex);

			if (!Loops.IsValidIndex(TaskIndex + 1)) { return false; }

			Group->InternalStartInlineRange<FGroupRangeInlineIterationTask>(TaskIndex + 1, Loops);
			Loops.Empty();
		}
		return true;
	}
}
