// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSubPointsBlendOperation.h"
#include "Data/Blending/PCGExMetadataBlender.h"

#include "PCGExSubPointsBlendInterpolate.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Path Blend Over Mode"))
enum class EPCGExPathBlendOver : uint8
{
	Distance UMETA(DisplayName = "Distance", ToolTip="Lerp is based on distance over total length"),
	Index UMETA(DisplayName = "Count", ToolTip="Lerp is based on point index over total count"),
	Fixed UMETA(DisplayName = "Fixed", ToolTip="Fixed Lerp value"),
};

/**
 * 
 */
UCLASS(DisplayName = "Interpolate")
class PCGEXTENDEDTOOLKIT_API UPCGExSubPointsBlendInterpolate : public UPCGExSubPointsBlendOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPathBlendOver BlendOver = EPCGExPathBlendOver::Distance;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="BlendOver==EPCGExPathBlendOver::Fixed", EditConditionHides))
	double Weight = 0.5;

	virtual void BlendSubPoints(
		const PCGEx::FPointRef& StartPoint,
		const PCGEx::FPointRef& EndPoint,
		TArrayView<FPCGPoint>& SubPoints,
		const PCGExMath::FPathMetricsSquared& Metrics,
		const PCGExDataBlending::FMetadataBlender* InBlender) const override;

	virtual PCGExDataBlending::FMetadataBlender* CreateBlender(PCGExData::FPointIO& InPrimaryIO, const PCGExData::FPointIO& InSecondaryIO, const PCGExData::ESource SecondarySource) override;

protected:
	virtual EPCGExDataBlendingType GetDefaultBlending() override;
	virtual void ApplyOverrides() override;
};
