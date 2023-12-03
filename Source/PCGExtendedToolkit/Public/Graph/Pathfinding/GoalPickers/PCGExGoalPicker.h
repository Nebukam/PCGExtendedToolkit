// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExInstruction.h"
#include "UObject/Object.h"
#include "PCGExGoalPicker.generated.h"

struct FPCGPoint;
class UPCGPointData;
/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew, DisplayName = "Default")
class PCGEXTENDEDTOOLKIT_API UPCGExGoalPicker : public UPCGExInstruction
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExIndexSafety IndexSafety = EPCGExIndexSafety::Wrap;
	
	virtual void PrepareForData(const UPCGPointData* InSeeds, const UPCGPointData* InGoals);
	virtual int32 GetGoalIndex(const FPCGPoint& Seed, const int32 SeedIndex) const;
	virtual void GetGoalIndices(const FPCGPoint& Seed, TArray<int32>& OutIndices) const;
	virtual bool OutputMultipleGoals() const;

protected:
	int32 MaxGoalIndex = -1;
	
};
