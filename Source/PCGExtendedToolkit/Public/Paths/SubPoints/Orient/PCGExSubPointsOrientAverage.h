// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSubPointsOrientOperation.h"
#include "PCGExSubPointsOrientAverage.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DisplayName = "Average")
class PCGEXTENDEDTOOLKIT_API UPCGExSubPointsOrientAverage : public UPCGExSubPointsOrientOperation
{
	GENERATED_BODY()

public:
	inline virtual void Orient(FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const override;
};
