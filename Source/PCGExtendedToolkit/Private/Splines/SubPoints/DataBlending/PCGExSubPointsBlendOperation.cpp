// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Splines/SubPoints/DataBlending/PCGExSubPointsBlendOperation.h"

#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExMetadataBlender.h"

EPCGExDataBlendingType UPCGExSubPointsBlendOperation::GetDefaultBlending()
{
	return EPCGExDataBlendingType::Copy;
}

void UPCGExSubPointsBlendOperation::PrepareForData(PCGExData::FPointIO& InData)
{
	Super::PrepareForData(InData);
	PrepareForData(
		InData.GetOut(), InData.GetOut(),
		InData.GetOutKeys(), InData.GetOutKeys());
}

void UPCGExSubPointsBlendOperation::PrepareForData(
	UPCGPointData* InPrimaryData,
	const UPCGPointData* InSecondaryData,
	FPCGAttributeAccessorKeysPoints* InPrimaryKeys,
	FPCGAttributeAccessorKeysPoints* InSecondaryKeys)
{
	PCGEX_DELETE(InternalBlender)
	//BlendingSettings.MakeWeightSafe();
	InternalBlender = CreateBlender(InPrimaryData, InSecondaryData, InPrimaryKeys, InSecondaryKeys);
	PropertiesBlender.Init(BlendingSettings);
}

void UPCGExSubPointsBlendOperation::ProcessSubPoints(const PCGEx::FPointRef& Start, const PCGEx::FPointRef& End, TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathMetrics& Metrics) const
{
	BlendSubPoints(Start, End, SubPoints, Metrics, InternalBlender);
}

void UPCGExSubPointsBlendOperation::ProcessSubPoints(TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathMetrics& Metrics) const
{
	BlendSubPoints(SubPoints, Metrics, InternalBlender);
}

void UPCGExSubPointsBlendOperation::ProcessSubPoints(TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathMetrics& PathInfos, const PCGExDataBlending::FMetadataBlender* InBlender) const
{
	BlendSubPoints(SubPoints, PathInfos, InBlender);
}

void UPCGExSubPointsBlendOperation::BlendSubPoints(
	const PCGEx::FPointRef& StartPoint,
	const PCGEx::FPointRef& EndPoint,
	TArrayView<FPCGPoint>& SubPoints,
	const PCGExMath::FPathMetrics& Metrics,
	const PCGExDataBlending::FMetadataBlender* InBlender) const
{
}

void UPCGExSubPointsBlendOperation::BlendSubPoints(TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathMetrics& Metrics, const PCGExDataBlending::FMetadataBlender* InBlender) const
{
	const FPCGPoint& Start = SubPoints[0];
	const int32 LastIndex = SubPoints.Num() - 1;
	const FPCGPoint& End = SubPoints[LastIndex];
	BlendSubPoints(PCGEx::FPointRef(Start, 0), PCGEx::FPointRef(End, LastIndex), SubPoints, Metrics, InBlender);
}

void UPCGExSubPointsBlendOperation::Cleanup()
{
	PCGEX_DELETE(InternalBlender)
	Super::Cleanup();
}

PCGExDataBlending::FMetadataBlender* UPCGExSubPointsBlendOperation::CreateBlender(
	UPCGPointData* InPrimaryData,
	const UPCGPointData* InSecondaryData,
	FPCGAttributeAccessorKeysPoints* InPrimaryKeys,
	FPCGAttributeAccessorKeysPoints* InSecondaryKeys)
{
	PCGExDataBlending::FMetadataBlender* NewBlender = new PCGExDataBlending::FMetadataBlender(GetDefaultBlending());
	NewBlender->PrepareForData(
		InPrimaryData,
		InSecondaryData,
		InPrimaryKeys,
		InSecondaryKeys,
		BlendingSettings.AttributesOverrides);

	return NewBlender;
}
