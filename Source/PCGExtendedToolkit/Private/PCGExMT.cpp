// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "PCGExMT.h"


namespace PCGExMT
{
	FTaskManager::~FTaskManager()
	{
		PCGEX_LOG_DTR(FTaskManager)
		Reset(true);
	}

	void FTaskManager::GrowNumStarted()
	{
		Context->bIsPaused = true;
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

		for (const TUniquePtr<FAsyncTaskBase>& Task : QueuedTasks)
		{
			if (Task && !Task->Cancel()) { Task->EnsureCompletion(); }
		}

		if (!bHoldStop) { FPlatformAtomics::InterlockedExchange(&Stopped, 0); }

		QueuedTasks.Empty();
		Groups.Empty();

		NumStarted = 0;
		NumCompleted = 0;

		Context->bIsPaused = false; // Just in case
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
			Context->bIsPaused = false; // Ensure context is unpaused
			return;
		}

		if (NumCompleted == NumStarted)
		{
			FPlatformAtomics::InterlockedExchange(&WorkComplete, 1);
			Context->bIsPaused = false; // Unpause context
		}
	}

	void FTaskGroup::StartRanges(const IterationCallback& Callback, const int32 MaxItems, const int32 ChunkSize, const bool bInlined, const bool bExecuteSmallSynchronously)
	{
		if (!Manager->IsAvailable()) { return; }

		OnIterationCallback = Callback;

		const int32 SanitizedChunkSize = FMath::Max(1, ChunkSize);

		if (MaxItems <= SanitizedChunkSize && bExecuteSmallSynchronously)
		{
			FPlatformAtomics::InterlockedAdd(&NumStarted, 1);
			if (OnIterationRangePrepareCallback) { OnIterationRangePrepareCallback({PCGEx::H64(0, MaxItems)}); }
			DoRangeIteration(0, MaxItems, 0);
			OnTaskCompleted();
			return;
		}

		if (bInlined)
		{
			TArray<uint64> Loops;
			FPlatformAtomics::InterlockedAdd(&NumStarted, SubRanges(Loops, MaxItems, SanitizedChunkSize));
			if (OnIterationRangePrepareCallback) { OnIterationRangePrepareCallback(Loops); }
			InternalStartInlineRange<FGroupRangeInlineIterationTask>(0, MaxItems, SanitizedChunkSize);
		}
		else
		{
			StartRanges<FGroupRangeIterationTask>(MaxItems, SanitizedChunkSize, nullptr);
		}
	}

	void FTaskGroup::PrepareRangesOnly(const int32 MaxItems, const int32 ChunkSize, const bool bInline)
	{
		if (!Manager->IsAvailable()) { return; }

		const int32 SanitizedChunkSize = FMath::Max(1, ChunkSize);

		if (bInline)
		{
			TArray<uint64> Loops;
			FPlatformAtomics::InterlockedAdd(&NumStarted, SubRanges(Loops, MaxItems, SanitizedChunkSize));
			if (OnIterationRangePrepareCallback) { OnIterationRangePrepareCallback(Loops); }
			InternalStartInlineRange<FGroupPrepareRangeInlineTask>(0, MaxItems, SanitizedChunkSize);
		}
		else { StartRanges<FGroupPrepareRangeTask>(MaxItems, SanitizedChunkSize, nullptr); }
	}

	void FTaskGroup::PrepareRangeIteration(const int32 StartIndex, const int32 Count, const int32 LoopIdx) const
	{
		if (!Manager->IsAvailable()) { return; }
		if (OnIterationRangeStartCallback) { OnIterationRangeStartCallback(StartIndex, Count, LoopIdx); }
	}

	void FTaskGroup::DoRangeIteration(const int32 StartIndex, const int32 Count, const int32 LoopIdx) const
	{
		if (!Manager->IsAvailable()) { return; }
		PrepareRangeIteration(StartIndex, Count, LoopIdx);
		for (int i = 0; i < Count; ++i) { OnIterationCallback(StartIndex + i, Count, LoopIdx); }
	}

	void FTaskGroup::OnTaskCompleted()
	{
		if (!Manager->IsAvailable()) { return; }

		{
			FWriteScopeLock WriteScopeLock(GroupLock);

			NumCompleted++;

			if (NumCompleted == NumStarted)
			{
				NumCompleted = 0;
				NumStarted = 0;
				if (OnCompleteCallback) { OnCompleteCallback(); }
				Manager->GrowNumCompleted();
			}
		}
	}

	void FPCGExTask::DoWork()
	{

		if (bWorkDone) { return; }
		bWorkDone = true;

		const TSharedPtr<FTaskManager> Manager = ManagerPtr.Pin();
		if (!Manager) { return; }
		if (!Manager->IsAvailable())
		{
			Manager->GrowNumCompleted();
			return;
		}

		const bool bResult = ExecuteTask(Manager);
		if (const TSharedPtr<FTaskGroup> Group = GroupPtr.Pin()) { Group->OnTaskCompleted(); }
		Manager->GrowNumCompleted();
	}

	bool FGroupRangeIterationTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		const TSharedPtr<FTaskGroup> Group = GroupPtr.Pin();
		if (!Group) { return false; }
		Group->DoRangeIteration(PCGEx::H64A(Scope), PCGEx::H64B(Scope), TaskIndex);
		return true;
	}

	bool FGroupPrepareRangeTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		const TSharedPtr<FTaskGroup> Group = GroupPtr.Pin();
		if (!Group) { return false; }
		Group->PrepareRangeIteration(PCGEx::H64A(Scope), PCGEx::H64B(Scope), TaskIndex);
		return true;
	}

	bool FGroupPrepareRangeInlineTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		const TSharedPtr<FTaskGroup> Group = GroupPtr.Pin();
		if (!Group) { return false; }

		TArray<uint64> Loops;
		SubRanges(Loops, MaxItems, ChunkSize);

		uint32 StartIndex;
		uint32 Count;
		PCGEx::H64(Loops[TaskIndex], StartIndex, Count);

		Group->PrepareRangeIteration(StartIndex, Count, TaskIndex);

		if (!Loops.IsValidIndex(TaskIndex + 1)) { return false; }

		Group->InternalStartInlineRange<FGroupPrepareRangeInlineTask>(TaskIndex + 1, MaxItems, ChunkSize);

		return true;
	}

	bool FGroupRangeInlineIterationTask::ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager)
	{
		const TSharedPtr<FTaskGroup> Group = GroupPtr.Pin();
		if (!Group) { return false; }

		TArray<uint64> Loops;
		SubRanges(Loops, MaxItems, ChunkSize);

		uint32 StartIndex;
		uint32 Count;
		PCGEx::H64(Loops[TaskIndex], StartIndex, Count);

		Group->DoRangeIteration(StartIndex, Count, TaskIndex);

		if (!Loops.IsValidIndex(TaskIndex + 1)) { return false; }

		Group->InternalStartInlineRange<FGroupRangeInlineIterationTask>(TaskIndex + 1, MaxItems, ChunkSize);

		return true;
	}
}
