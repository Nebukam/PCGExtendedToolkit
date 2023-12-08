// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "PCGExMT.h"

UPCGExAsyncTaskManager::~UPCGExAsyncTaskManager()
{
}

void UPCGExAsyncTaskManager::OnAsyncTaskExecutionComplete(FPCGExAsyncTask* AsyncTask, bool bSuccess)
{
	FWriteScopeLock WriteLock(ManagerLock);
	NumCompleted++;
}

bool UPCGExAsyncTaskManager::IsAsyncWorkComplete() const
{
	FReadScopeLock ReadLock(ManagerLock);
	//UE_LOG(LogTemp, Warning, TEXT(" %d / %d"), NumCompleted, NumStarted);
	return NumCompleted == NumStarted;
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
