// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/



#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeRWLock.h"

class PCGEXCORE_API FPCGExIntTracker final : public TSharedFromThis<FPCGExIntTracker>
{
	FRWLock Lock;
	bool bTriggered = false;
	int32 PendingCount = 0;
	int32 CompletedCount = 0;

	TFunction<void()> StartFn = nullptr;
	TFunction<void()> ThresholdFn = nullptr;

public:
	explicit FPCGExIntTracker(TFunction<void()>&& InThresholdFn)
	{
		ThresholdFn = InThresholdFn;
	}

	explicit FPCGExIntTracker(TFunction<void()>&& InStartFn, TFunction<void()>&& InThresholdFn)
	{
		StartFn = InStartFn;
		ThresholdFn = InThresholdFn;
	}

	~FPCGExIntTracker() = default;

	void IncrementPending(const int32 Count = 1);
	void IncrementCompleted(const int32 Count = 1);

	void Trigger();
	void SafetyTrigger();

	void Reset();
	void Reset(const int32 InMax);

protected:
	void TriggerInternal();
};
