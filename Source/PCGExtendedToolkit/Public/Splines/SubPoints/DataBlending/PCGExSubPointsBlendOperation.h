﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGPoint.h"
#include "Data/Blending/PCGExPropertiesBlender.h"
#include "Splines/SubPoints/PCGExSubPointsOperation.h"
#include "PCGExSubPointsBlendOperation.generated.h"

namespace PCGExDataBlending
{
	class FMetadataBlender;
}

/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExSubPointsBlendOperation : public UPCGExSubPointsOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ShowOnlyInnerProperties))
	FPCGExBlendingSettings BlendingSettings;

	virtual void PrepareForData(PCGExData::FPointIO& InData) override;
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
		const PCGExMath::FPathMetrics& Metrics) const override;

	virtual void ProcessSubPoints(
		TArrayView<FPCGPoint>& SubPoints,
		const PCGExMath::FPathMetrics& PathInfos,
		const PCGExDataBlending::FMetadataBlender* InBlender) const;

	virtual void BlendSubPoints(
		const PCGEx::FPointRef& StartPoint,
		const PCGEx::FPointRef& EndPoint,
		TArrayView<FPCGPoint>& SubPoints,
		const PCGExMath::FPathMetrics& Metrics,
		const PCGExDataBlending::FMetadataBlender* InBlender) const;

	virtual void BlendSubPoints(
		TArrayView<FPCGPoint>& SubPoints,
		const PCGExMath::FPathMetrics& Metrics,
		const PCGExDataBlending::FMetadataBlender* InBlender) const;


	virtual void BeginDestroy() override;

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