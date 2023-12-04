// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "PCGExMT.h"

void UPCGExAsyncTaskManager::OnAsyncTaskExecutionComplete(FPCGExAsyncTask* AsyncTask, bool bSuccess)
{
	if (!IsValid()) { return; }
	FWriteScopeLock WriteLock(AsyncUpdateLock);
	NumAsyncTaskCompleted++;
}

bool UPCGExAsyncTaskManager::IsAsyncWorkComplete()
{
	FReadScopeLock ReadLock(AsyncCreateLock);
	FReadScopeLock OtherReadLock(AsyncUpdateLock);
	return NumAsyncTaskStarted == NumAsyncTaskCompleted;
}

bool UPCGExAsyncTaskManager::IsValid()
{
	if (!Context ||
		(Context->SourceComponent == nullptr || !Context->SourceComponent.IsValid() || Context->SourceComponent.IsStale(true, true)) ||
		NumAsyncTaskStarted == 0)
	{
		return false;
	}

	return true;
}
