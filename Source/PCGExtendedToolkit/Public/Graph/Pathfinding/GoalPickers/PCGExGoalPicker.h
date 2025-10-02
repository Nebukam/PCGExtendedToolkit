﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExInstancedFactory.h"
#include "PCGExMath.h"

#include "UObject/Object.h"
#include "PCGExGoalPicker.generated.h"

namespace PCGExData
{
	struct FConstPoint;
}

/**
 * 
 */
UCLASS(meta=(DisplayName = "Default", PCGExNodeLibraryDoc="pathfinding/pathfinding-edges/goal-picker-default"))
class PCGEXTENDEDTOOLKIT_API UPCGExGoalPicker : public UPCGExInstancedFactory
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExIndexSafety IndexSafety = EPCGExIndexSafety::Tile;

	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override;

	virtual bool PrepareForData(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InSeedsDataFacade, const TSharedPtr<PCGExData::FFacade>& InGoalsDataFacade);
	virtual int32 GetGoalIndex(const PCGExData::FConstPoint& Seed) const;
	virtual void GetGoalIndices(const PCGExData::FConstPoint& Seed, TArray<int32>& OutIndices) const;
	virtual bool OutputMultipleGoals() const;

protected:
	int32 MaxGoalIndex = -1;
};
