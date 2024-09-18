// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "PCGExMT.h"

namespace PCGExMT
{
	FTaskManager::~FTaskManager()
	{
		FPlatformAtomics::InterlockedExchange(&Stopped, 1);
		Reset();
	}

	FTaskGroup* FTaskManager::CreateGroup(const FName& GroupName)
	{
		check(IsAvailable())

		FTaskGroup* NewGroup = new FTaskGroup(this, GroupName);
		{
			FWriteScopeLock WriteLock(GroupLock);
			Groups.Add(NewGroup);
		}
		return NewGroup;
	}

	void FTaskManager::OnAsyncTaskExecutionComplete(FPCGExTask* AsyncTask, bool bSuccess)
	{
		if (Flushing) { return; }
		FPlatformAtomics::InterlockedAdd(&NumCompleted, 1);
		if (NumCompleted == NumStarted) { Context->bIsPaused = false; }
	}

	bool FTaskManager::IsAsyncWorkComplete() const
	{
		return NumCompleted == NumStarted;
	}

	void FTaskManager::Reset()
	{
		FPlatformAtomics::InterlockedExchange(&Stopped, 1);

		FWriteScopeLock WriteLock(ManagerLock);

		for (FAsyncTaskBase* Task : QueuedTasks)
		{
			if (Task && !Task->Cancel()) { Task->EnsureCompletion(); }
			delete Task;
		}

		FPlatformAtomics::InterlockedExchange(&Stopped, 0);

		QueuedTasks.Empty();
		NumStarted = 0;
		NumCompleted = 0;

		PCGEX_DELETE_TARRAY(Groups)
	}

	void FTaskGroup::StartRanges(const IterationCallback& Callback, const int32 MaxItems, const int32 ChunkSize, const bool bInlined, const bool bExecuteSmallSynchronously)
	{
		if (!Manager->IsAvailable()) { return; }

		OnIterationCallback = Callback;

		const int32 SanitizedChunkSize = FMath::Max(1, ChunkSize);

		if (MaxItems <= SanitizedChunkSize && bExecuteSmallSynchronously)
		{
			FPlatformAtomics::InterlockedAdd(&NumStarted, 1);
			if (bHasOnIterationRangePrepareCallback) { OnIterationRangePrepareCallback({PCGEx::H64(0, MaxItems)}); }
			DoRangeIteration(0, MaxItems, 0);
			OnTaskCompleted();
			return;
		}

		if (bInlined)
		{
			TArray<uint64> Loops;
			FPlatformAtomics::InterlockedAdd(&NumStarted, SubRanges(Loops, MaxItems, SanitizedChunkSize));
			if (bHasOnIterationRangePrepareCallback) { OnIterationRangePrepareCallback(Loops); }
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
			if (bHasOnIterationRangePrepareCallback) { OnIterationRangePrepareCallback(Loops); }
			InternalStartInlineRange<FGroupPrepareRangeInlineTask>(0, MaxItems, SanitizedChunkSize);
		}
		else { StartRanges<FGroupPrepareRangeTask>(MaxItems, SanitizedChunkSize, nullptr); }
	}

	void FTaskGroup::PrepareRangeIteration(const int32 StartIndex, const int32 Count, const int32 LoopIdx) const
	{
		if (!Manager->IsAvailable()) { return; }
		if (bHasOnIterationRangeStartCallback) { OnIterationRangeStartCallback(StartIndex, Count, LoopIdx); }
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
				if (bHasOnCompleteCallback) { OnCompleteCallback(); }
			}
		}
	}

	bool FGroupRangeIterationTask::ExecuteTask()
	{
		check(Group)
		Group->DoRangeIteration(PCGEx::H64A(Scope), PCGEx::H64B(Scope), TaskIndex);
		return true;
	}

	bool FGroupPrepareRangeTask::ExecuteTask()
	{
		check(Group)
		Group->PrepareRangeIteration(PCGEx::H64A(Scope), PCGEx::H64B(Scope), TaskIndex);
		return true;
	}

	bool FGroupPrepareRangeInlineTask::ExecuteTask()
	{
		check(Group)

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

	bool FGroupRangeInlineIterationTask::ExecuteTask()
	{
		check(Group)

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
