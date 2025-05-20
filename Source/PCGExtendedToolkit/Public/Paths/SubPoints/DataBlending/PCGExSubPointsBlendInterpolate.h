// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSubPointsBlendOperation.h"
#include "Data/Blending/PCGExMetadataBlender.h"




#include "PCGExSubPointsBlendInterpolate.generated.h"


class UPCGExSubPointsBlendInterpolate;

class FPCGExSubPointsBlendInterpolate : public FPCGExSubPointsBlendOperation
{
public:
	const UPCGExSubPointsBlendInterpolate* TypedFactory = nullptr;

	virtual void BlendSubPoints(
		const PCGExData::FConstPoint& From, const PCGExData::FConstPoint& To,
		const TArrayView<FPCGPoint>& SubPoints, const PCGExPaths::FPathMetrics& Metrics, const int32 StartIndex = -1) const override;
};

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Interpolate")
class UPCGExSubPointsBlendInterpolate : public UPCGExSubPointsBlendInstancedFactory
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExBlendOver BlendOver = EPCGExBlendOver::Distance;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="BlendOver==EPCGExBlendOver::Fixed", EditConditionHides))
	double Lerp = 0.5;

	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override;

	virtual TSharedPtr<FPCGExSubPointsBlendOperation> CreateOperation() const override;
};
