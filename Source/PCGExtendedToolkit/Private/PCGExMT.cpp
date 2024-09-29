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

	TSharedPtr<FTaskGroup> FTaskManager::CreateGroup(const FName& GroupName)
	{
		check(IsAvailable())

		{
			FWriteScopeLock WriteLock(GroupLock);
			return Groups.Add_GetRef(MakeShared<FTaskGroup>(SharedThis(this), GroupName));
		}
	}

	void FTaskManager::OnAsyncTaskExecutionComplete(FPCGExTask* AsyncTask, bool bSuccess)
	{
		if (Flushing) { return; }
		FPlatformAtomics::InterlockedAdd(&NumCompleted, 1);
		if (NumCompleted == NumStarted) { ScheduleUnpause(); }
	}

	bool FTaskManager::IsAsyncWorkComplete() const
	{
		return WorkComplete > 0;
	}

	void FTaskManager::Reset(const bool bStop)
	{
		if (Stopped) { return; }

		FPlatformAtomics::InterlockedExchange(&PauseScheduled, 0);
		FPlatformAtomics::InterlockedExchange(&WorkComplete, 0);

		FPlatformAtomics::InterlockedExchange(&Stopped, 1);

		FWriteScopeLock WriteLock(ManagerLock);

		for (const TUniquePtr<FAsyncTaskBase>& Task : QueuedTasks)
		{
			if (Task && !Task->Cancel()) { Task->EnsureCompletion(); }
		}

		if (!bStop) { FPlatformAtomics::InterlockedExchange(&Stopped, 0); }

		QueuedTasks.Empty();
		NumStarted = 0;
		NumCompleted = 0;

		Groups.Empty();
	}

	void FTaskManager::ScheduleUnpause()
	{
		if (PauseScheduled) { return; }

		FPlatformAtomics::InterlockedExchange(&PauseScheduled, 1);

		const TSharedPtr<FTaskManager> SharedSelf = SharedThis(this);
		TWeakPtr<FTaskManager> WeakThisPtr = SharedSelf;

		AsyncTask(
			ENamedThreads::BackgroundThreadPriority, [WeakThisPtr]()
			{
				const TSharedPtr<FTaskManager> Manager = WeakThisPtr.Pin();
				if (!Manager || !Manager->IsAvailable()) { return; }
				Manager->TryUnpause();
			});
	}

	void FTaskManager::TryUnpause()
	{
		if (!PauseScheduled) { return; }
		FPlatformAtomics::InterlockedExchange(&PauseScheduled, 0);

		if (Flushing || Stopped) { return; }

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
			}
		}
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
