// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSubPointsBlendOperation.h"
#include "Data/Blending/PCGExMetadataBlender.h"


#include "PCGExSubPointsBlendInterpolate.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Interpolate")
class UPCGExSubPointsBlendInterpolate : public UPCGExSubPointsBlendOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExBlendOver BlendOver = EPCGExBlendOver::Distance;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="BlendOver==EPCGExBlendOver::Fixed", EditConditionHides))
	double Lerp = 0.5;

	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override;

	virtual void BlendSubPoints(
		const PCGExData::FConstPoint& From,
		const PCGExData::FConstPoint& To,
		const TArrayView<FPCGPoint>& SubPoints,
		const PCGExPaths::FPathMetrics& Metrics,
		PCGExDataBlending::FMetadataBlender* InBlender,
		const int32 StartIndex) const override;

	virtual TSharedPtr<PCGExDataBlending::FMetadataBlender> CreateBlender(const TSharedRef<PCGExData::FFacade>& InTargetFacade, const TSharedRef<PCGExData::FFacade>& InSourceFacade, const PCGExData::EIOSide InSourceSide, const TSet<FName>* IgnoreAttributeSet) override;

protected:
	virtual EPCGExDataBlendingType GetDefaultBlending() override { return EPCGExDataBlendingType::Lerp; }
};
