// Copyright Timothé Lapetite 2023
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
UCLASS(Abstract, BlueprintType, Blueprintable)
class PCGEXTENDEDTOOLKIT_API UPCGExSubPointsBlendOperation : public UPCGExSubPointsOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExBlendingSettings BlendingSettings;

	virtual void PrepareForData(PCGExData::FPointIO& InPointIO) override;
	virtual void PrepareForData(
		UPCGPointData* InPrimaryData,
		const UPCGPointData* InSecondaryData,
		FPCGAttributeAccessorKeysPoints* InPrimaryKeys = nullptr,
		FPCGAttributeAccessorKeysPoints* InSecondaryKeys = nullptr);

	virtual void ProcessSubPoints(
		const PCGEx::FPointRef& Start,
		const PCGEx::FPointRef& End,
		TArrayView<FPCGPoint>& SubPoints,
		const PCGExMath::FPathMetrics& Metrics) const override;

	virtual void ProcessSubPoints(
		TArrayView<FPCGPoint>& SubPoints,
		const PCGExMath::FPathMetrics& Metrics,
		const int32 Offset = 0) const override;

	virtual void ProcessSubPoints(
		TArrayView<FPCGPoint>& SubPoints,
		const PCGExMath::FPathMetrics& PathInfos,
		const PCGExDataBlending::FMetadataBlender* InBlender,
		const int32 Offset = 0) const;

	virtual void BlendSubPoints(
		const PCGEx::FPointRef& StartPoint,
		const PCGEx::FPointRef& EndPoint,
		TArrayView<FPCGPoint>& SubPoints,
		const PCGExMath::FPathMetrics& Metrics,
		const PCGExDataBlending::FMetadataBlender* InBlender) const;

	virtual void BlendSubPoints(
		TArrayView<FPCGPoint>& SubPoints,
		const PCGExMath::FPathMetrics& Metrics,
		const PCGExDataBlending::FMetadataBlender* InBlender,
		const int32 Offset = 0) const;

	virtual void Cleanup() override;

	PCGExDataBlending::FMetadataBlender* CreateBlender(
		UPCGPointData* InPrimaryData,
		const UPCGPointData* InSecondaryData,
		FPCGAttributeAccessorKeysPoints* InPrimaryKeys = nullptr,
		FPCGAttributeAccessorKeysPoints* InSecondaryKeys = nullptr);

protected:
	virtual EPCGExDataBlendingType GetDefaultBlending();
	PCGExDataBlending::FMetadataBlender* InternalBlender;
	PCGExDataBlending::FPropertiesBlender PropertiesBlender;
};
