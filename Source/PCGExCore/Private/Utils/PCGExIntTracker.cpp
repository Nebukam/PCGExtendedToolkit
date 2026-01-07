// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Utils/PCGExIntTracker.h"
#include "CoreMinimal.h"

void FPCGExIntTracker::IncrementPending(const int32 Count)
{
	{
		FReadScopeLock ReadScopeLock(Lock);
		if (bTriggered) { return; }
	}
	{
		FWriteScopeLock WriteScopeLock(Lock);
		if (PendingCount == 0 && StartFn) { StartFn(); }
		PendingCount += Count;
	}
}

void FPCGExIntTracker::IncrementCompleted(const int32 Count)
{
	{
		FReadScopeLock ReadScopeLock(Lock);
		if (bTriggered) { return; }
	}
	{
		FWriteScopeLock WriteScopeLock(Lock);
		CompletedCount += Count;
		if (CompletedCount == PendingCount) { TriggerInternal(); }
	}
}

void FPCGExIntTracker::Trigger()
{
	FWriteScopeLock WriteScopeLock(Lock);
	TriggerInternal();
}

void FPCGExIntTracker::SafetyTrigger()
{
	FWriteScopeLock WriteScopeLock(Lock);
	if (PendingCount > 0) { TriggerInternal(); }
}

void FPCGExIntTracker::Reset()
{
	FWriteScopeLock WriteScopeLock(Lock);
	PendingCount = CompletedCount = 0;
	bTriggered = false;
}

void FPCGExIntTracker::Reset(const int32 InMax)
{
	FWriteScopeLock WriteScopeLock(Lock);
	PendingCount = InMax;
	CompletedCount = 0;
	bTriggered = false;
}

void FPCGExIntTracker::TriggerInternal()
{
	if (bTriggered) { return; }
	bTriggered = true;
	ThresholdFn();
	PendingCount = CompletedCount = 0;
}
