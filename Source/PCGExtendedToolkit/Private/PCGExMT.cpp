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
}
