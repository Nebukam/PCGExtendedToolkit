// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSubPointsOrient.h"
#include "PCGExSubPointsOrientWeighted.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew, DisplayName = "Weighted")
class PCGEXTENDEDTOOLKIT_API UPCGExSubPointsOrientWeighted : public UPCGExSubPointsOrient
{
	GENERATED_BODY()

public:
	UPROPERTY()
	bool bInverseWeight = false;

	virtual void ProcessSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const double PathLength) const override;
	
};
