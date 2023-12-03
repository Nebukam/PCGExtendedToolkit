// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSubPointsDataBlend.h"
#include "PCGExSubPointsDataBlendLerp.generated.h"

UENUM(BlueprintType)
enum class EPCGExPathLerpBase : uint8
{
	Distance UMETA(DisplayName = "Distance", ToolTip="Lerp is based on distance over total length"),
	Index UMETA(DisplayName = "Count", ToolTip="Lerp is based on point index over total count"),
	Fixed UMETA(DisplayName = "Fixed", ToolTip="Fixed Lerp value"),
};

/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew, DisplayName = "Interpolate")
class PCGEXTENDEDTOOLKIT_API UPCGExSubPointsDataBlendLerp : public UPCGExSubPointsDataBlend
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExPathLerpBase LerpBase = EPCGExPathLerpBase::Distance;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="LerpBase==EPCGExPathLerpBase::Fixed", EditConditionHides))
	double Alpha = 0.5;

	virtual void ProcessSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const double PathLength) const override;
};
