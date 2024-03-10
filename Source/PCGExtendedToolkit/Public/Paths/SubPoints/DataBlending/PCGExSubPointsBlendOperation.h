// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGPoint.h"
#include "Data/Blending/PCGExPropertiesBlender.h"
#include "Paths/SubPoints/PCGExSubPointsOperation.h"
#include "PCGExSubPointsBlendOperation.generated.h"

namespace PCGExDataBlending
{
	class FMetadataBlender;
}

/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExSubPointsBlendOperation : public UPCGExSubPointsOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExBlendingSettings BlendingSettings;

	virtual void PrepareForData(PCGExData::FPointIO& InPointIO) override;
	virtual void PrepareForData(PCGExData::FPointIO& InPrimaryData, const PCGExData::FPointIO& InSecondaryData, bool bSecondaryIn);

	virtual void ProcessSubPoints(
		const PCGEx::FPointRef& Start,
		const PCGEx::FPointRef& End,
		TArrayView<FPCGPoint>& SubPoints,
		const PCGExMath::FPathMetricsSquared& Metrics) const override;

	virtual void ProcessSubPoints(
		TArrayView<FPCGPoint>& SubPoints,
		const PCGExMath::FPathMetricsSquared& Metrics,
		const int32 Offset = 0) const override;

	virtual void ProcessSubPoints(
		TArrayView<FPCGPoint>& SubPoints,
		const PCGExMath::FPathMetricsSquared& PathInfos,
		const PCGExDataBlending::FMetadataBlender* InBlender,
		const int32 Offset = 0) const;

	virtual void BlendSubPoints(
		const PCGEx::FPointRef& StartPoint,
		const PCGEx::FPointRef& EndPoint,
		TArrayView<FPCGPoint>& SubPoints,
		const PCGExMath::FPathMetricsSquared& Metrics,
		const PCGExDataBlending::FMetadataBlender* InBlender) const;

	virtual void BlendSubPoints(
		TArrayView<FPCGPoint>& SubPoints,
		const PCGExMath::FPathMetricsSquared& Metrics,
		const PCGExDataBlending::FMetadataBlender* InBlender,
		const int32 Offset = 0) const;

	virtual void Write() override;
	virtual void Cleanup() override;

	virtual PCGExDataBlending::FMetadataBlender* CreateBlender(PCGExData::FPointIO& InPrimaryIO, const PCGExData::FPointIO& InSecondaryIO, bool bSecondaryIn = true);

protected:
	virtual EPCGExDataBlendingType GetDefaultBlending();
	PCGExDataBlending::FMetadataBlender* InternalBlender;
};
