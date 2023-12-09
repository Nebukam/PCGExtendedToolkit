// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "PCGExMT.h"

UPCGExAsyncTaskManager::~UPCGExAsyncTaskManager()
{
	bStopped = true;
	Reset();
}

void UPCGExAsyncTaskManager::OnAsyncTaskExecutionComplete(FPCGExAsyncTask* AsyncTask, bool bSuccess)
{
	if (bFlushing) { return; }
	FWriteScopeLock WriteLock(ManagerLock);
	NumCompleted++;
}

bool UPCGExAsyncTaskManager::IsAsyncWorkComplete() const
{
	FReadScopeLock ReadLock(ManagerLock);
	return NumCompleted == NumStarted;
}

void UPCGExAsyncTaskManager::Reset()
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

bool UPCGExAsyncTaskManager::IsValid() const
{
	if (!Context ||
		Context->SourceComponent == nullptr ||
		!Context->SourceComponent.IsValid() ||
		Context->SourceComponent.IsStale(true, true) ||
		NumStarted == 0)
	{
		return false;
	}

	return true;
}
