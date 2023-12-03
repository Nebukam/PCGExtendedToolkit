// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGPoint.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Splines/SubPoints/PCGExSubPointsProcessor.h"
#include "PCGExSubPointsDataBlend.generated.h"

class UPCGExPointIO;

/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSubPointsDataBlend : public UPCGExSubPointsProcessor
{
	GENERATED_BODY()

public:
	virtual void PrepareForData(const UPCGExPointIO* InData, PCGEx::FAttributeMap* InAttributeMap) override;
	virtual void ProcessSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const double PathLength) const override;
};
