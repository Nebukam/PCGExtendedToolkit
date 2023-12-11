// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSubPointsBlendOperation.h"
#include "Data/Blending/PCGExMetadataBlender.h"

#include "PCGExSubPointsBlendInterpolate.generated.h"

UENUM(BlueprintType)
enum class EPCGExPathBlendOver : uint8
{
	Distance UMETA(DisplayName = "Distance", ToolTip="Lerp is based on distance over total length"),
	Index UMETA(DisplayName = "Count", ToolTip="Lerp is based on point index over total count"),
	Fixed UMETA(DisplayName = "Fixed", ToolTip="Fixed Lerp value"),
};

/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew, DisplayName = "Interpolate")
class PCGEXTENDEDTOOLKIT_API UPCGExSubPointsBlendInterpolate : public UPCGExSubPointsBlendOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExPathBlendOver BlendOver = EPCGExPathBlendOver::Distance;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="BlendOver==EPCGExPathBlendOver::Fixed", EditConditionHides))
	double Alpha = 0.5;

	virtual void BlendSubPoints(
		const PCGEx::FPointRef& StartPoint,
		const PCGEx::FPointRef& EndPoint,
		TArrayView<FPCGPoint>& SubPoints,
		const PCGExMath::FPathMetrics& Metrics,
		const PCGExDataBlending::FMetadataBlender* InBlender) const override;

protected:
	virtual EPCGExDataBlendingType GetDefaultBlending() override;
};
