// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExOperation.h"


#include "UObject/Object.h"
#include "PCGExGoalPicker.generated.h"

struct FPCGPoint;
class UPCGPointData;
/**
 * 
 */
UCLASS(DisplayName = "Default")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExGoalPicker : public UPCGExOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = GoalPicker)
	EPCGExIndexSafety IndexSafety = EPCGExIndexSafety::Tile;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual void PrepareForData(const TSharedPtr<PCGExData::FFacade>& InSeedsDataFacade, const TSharedPtr<PCGExData::FFacade>& InGoalsDataFacade);
	virtual int32 GetGoalIndex(const PCGExData::FPointRef& Seed) const;
	virtual void GetGoalIndices(const PCGExData::FPointRef& Seed, TArray<int32>& OutIndices) const;
	virtual bool OutputMultipleGoals() const;

protected:
	int32 MaxGoalIndex = -1;
};
