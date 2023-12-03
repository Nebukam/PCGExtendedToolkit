// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSubPointsOrient.h"
#include "PCGExSubPointsOrientAverage.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew, DisplayName = "Average")
class PCGEXTENDEDTOOLKIT_API UPCGExSubPointsOrientAverage : public UPCGExSubPointsOrient
{
	GENERATED_BODY()
	
public:
	virtual void ProcessSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const double PathLength) const override;
};
