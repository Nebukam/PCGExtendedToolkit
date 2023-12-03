// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSubPointsDataBlend.h"
#include "PCGExSubPointsDataBlendInheritStart.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew, DisplayName = "Inherit Start")
class PCGEXTENDEDTOOLKIT_API UPCGExSubPointsDataBlendInheritStart : public UPCGExSubPointsDataBlend
{
	GENERATED_BODY()
public:
	virtual void ProcessSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const double PathLength) const override;
};
