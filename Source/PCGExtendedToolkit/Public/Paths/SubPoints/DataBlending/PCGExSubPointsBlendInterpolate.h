// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSubPointsBlendOperation.h"
#include "Data/Blending/PCGExMetadataBlender.h"

#include "PCGExSubPointsBlendInterpolate.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Interpolate")
class PCGEXTENDEDTOOLKIT_API UPCGExSubPointsBlendInterpolate : public UPCGExSubPointsBlendOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExBlendOver BlendOver = EPCGExBlendOver::Distance;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="BlendOver==EPCGExPathBlendOver::Fixed", EditConditionHides))
	double Weight = 0.5;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual void BlendSubPoints(
		const PCGEx::FPointRef& StartPoint,
		const PCGEx::FPointRef& EndPoint,
		const TArrayView<FPCGPoint>& SubPoints,
		const PCGExMath::FPathMetricsSquared& Metrics,
		PCGExDataBlending::FMetadataBlender* InBlender) const override;

	virtual PCGExDataBlending::FMetadataBlender* CreateBlender(PCGExData::FFacade* InPrimaryFacade, PCGExData::FFacade* InSecondaryFacade, const PCGExData::ESource SecondarySource) override;

protected:
	virtual EPCGExDataBlendingType GetDefaultBlending() override;
	virtual void ApplyOverrides() override;
};
