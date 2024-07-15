// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "PCGExMT.h"

namespace PCGExMT
{
	FTaskManager::~FTaskManager()
	{
		bStopped = true;
		PCGEX_DELETE_TARRAY(Groups)
		Reset();
	}

	FTaskGroup* FTaskManager::CreateGroup()
	{
		FWriteScopeLock WriteScopeLock(ManagerLock);
		FTaskGroup* NewGroup = new FTaskGroup(this);
		Groups.Add(NewGroup);
		return NewGroup;
	}

	void FTaskManager::OnAsyncTaskExecutionComplete(FPCGExTask* AsyncTask, bool bSuccess)
	{
		if (bFlushing) { return; }
		FWriteScopeLock WriteLock(ManagerLock);
		NumCompleted++;
	}

	bool FTaskManager::IsAsyncWorkComplete() const
	{
		FReadScopeLock ReadLock(ManagerLock);
		return NumCompleted == NumStarted;
	}

	void FTaskManager::Reset()
	{
		FWriteScopeLock WriteLock(ManagerLock);

		bFlushing = true;
		for (FAsyncTaskBase* Task : QueuedTasks)
		{
			if (Task && !Task->Cancel()) { Task->EnsureCompletion(); }
			delete Task;
		}
		bFlushing = false;

		QueuedTasks.Empty();
		NumStarted = 0;
		NumCompleted = 0;
	}

	void FTaskGroup::StartRanges(const IterationCallback& Callback, const int32 MaxItems, const int32 ChunkSize, const bool bInlined)
	{
		OnIterationCallback = Callback;
		if (bInlined)
		{
			TArray<uint64> Loops;
			NumStarted += SubRanges(Loops, MaxItems, ChunkSize);
			InternalStartInlineRange(0, MaxItems, ChunkSize);
		}
		else
		{
			StartRanges<FGroupRangeIterationTask>(MaxItems, ChunkSize, nullptr);
		}
	}

	void FTaskGroup::PrepareRangesOnly(const int32 MaxItems, const int32 ChunkSize)
	{
		StartRanges<FGroupPrepareRangeTask>(MaxItems, ChunkSize, nullptr);
	}

	void FTaskGroup::DoRangeIteration(const int32 StartIndex, const int32 Count, const int32 LoopIdx) const
	{
		PrepareRangeIteration(StartIndex, Count, LoopIdx);
		for (int i = 0; i < Count; i++) { OnIterationCallback(StartIndex + i, Count, LoopIdx); }
	}

	void FTaskGroup::PrepareRangeIteration(const int32 StartIndex, const int32 Count, const int32 LoopIdx) const
	{
		if (bHasOnIterationRangeStartCallback) { OnIterationRangeStartCallback(StartIndex, Count, LoopIdx); }
	}

	void FTaskGroup::InternalStartInlineRange(const int32 Index, const int32 MaxItems, const int32 ChunkSize)
	{
		FAsyncTask<FGroupRangeInlineIterationTask>* NextRange = new FAsyncTask<FGroupRangeInlineIterationTask>(nullptr);
		NextRange->GetTask().Group = this;
		NextRange->GetTask().MaxItems = MaxItems;
		NextRange->GetTask().ChunkSize = ChunkSize;

		if (Manager->bForceSync) { Manager->StartSynchronousTask<FGroupRangeInlineIterationTask>(NextRange, Index); }
		else { Manager->StartBackgroundTask<FGroupRangeInlineIterationTask>(NextRange, Index); }
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

		Group->InternalStartInlineRange(TaskIndex + 1, MaxItems, ChunkSize);

		return true;
	}
}
