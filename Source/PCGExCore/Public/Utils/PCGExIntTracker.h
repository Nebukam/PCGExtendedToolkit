// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

// Most helpers here are Copyright Epic Games, Inc. All Rights Reserved, cherry picked for 5.3

#pragma once

#include "CoreMinimal.h"

class PCGEXCORE_API FIntTracker final : public TSharedFromThis<FIntTracker>
{
	FRWLock Lock;
	bool bTriggered = false;
	int32 PendingCount = 0;
	int32 CompletedCount = 0;

	TFunction<void()> StartFn = nullptr;
	TFunction<void()> ThresholdFn = nullptr;

public:
	explicit FIntTracker(TFunction<void()>&& InThresholdFn)
	{
		ThresholdFn = InThresholdFn;
	}

	explicit FIntTracker(TFunction<void()>&& InStartFn, TFunction<void()>&& InThresholdFn)
	{
		StartFn = InStartFn;
		ThresholdFn = InThresholdFn;
	}

	~FIntTracker() = default;

	void IncrementPending(const int32 Count = 1);
	void IncrementCompleted(const int32 Count = 1);

	void Trigger();
	void SafetyTrigger();

	void Reset();
	void Reset(const int32 InMax);

protected:
	void TriggerInternal();
};
