// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "PCGExMT.h"

FPCGExAsyncManager::~FPCGExAsyncManager()
{
	bStopped = true;
	Reset();
}

void FPCGExAsyncManager::OnAsyncTaskExecutionComplete(FPCGExNonAbandonableTask* AsyncTask, bool bSuccess)
{
	if (bFlushing) { return; }
	FWriteScopeLock WriteLock(ManagerLock);
	NumCompleted++;
}

bool FPCGExAsyncManager::IsAsyncWorkComplete() const
{
	FReadScopeLock ReadLock(ManagerLock);
	return NumCompleted == NumStarted;
}

void FPCGExAsyncManager::Reset()
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
