// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendOperation.h"

#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExMetadataBlender.h"

EPCGExDataBlendingType UPCGExSubPointsBlendOperation::GetDefaultBlending()
{
	return EPCGExDataBlendingType::Weight;
}

void UPCGExSubPointsBlendOperation::PrepareForData(PCGExData::FPointIO& InPointIO)
{
	Super::PrepareForData(InPointIO);
	PrepareForData(InPointIO, InPointIO, true);
}

void UPCGExSubPointsBlendOperation::PrepareForData(
	PCGExData::FPointIO& InPrimaryData, const PCGExData::FPointIO& InSecondaryData, const bool bSecondaryIn)
{
	PCGEX_DELETE(InternalBlender)
	InternalBlender = CreateBlender(InPrimaryData, InSecondaryData, bSecondaryIn);
}

void UPCGExSubPointsBlendOperation::ProcessSubPoints(const PCGEx::FPointRef& Start, const PCGEx::FPointRef& End, TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathMetricsSquared& Metrics) const
{
	BlendSubPoints(Start, End, SubPoints, Metrics, InternalBlender);
}

void UPCGExSubPointsBlendOperation::ProcessSubPoints(TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathMetricsSquared& Metrics, const int32 Offset) const
{
	BlendSubPoints(SubPoints, Metrics, InternalBlender, Offset);
}

void UPCGExSubPointsBlendOperation::ProcessSubPoints(TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathMetricsSquared& PathInfos, const PCGExDataBlending::FMetadataBlender* InBlender, const int32 Offset) const
{
	BlendSubPoints(SubPoints, PathInfos, InBlender, Offset);
}

void UPCGExSubPointsBlendOperation::BlendSubPoints(
	const PCGEx::FPointRef& StartPoint,
	const PCGEx::FPointRef& EndPoint,
	TArrayView<FPCGPoint>& SubPoints,
	const PCGExMath::FPathMetricsSquared& Metrics,
	const PCGExDataBlending::FMetadataBlender* InBlender) const
{
}

void UPCGExSubPointsBlendOperation::BlendSubPoints(TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathMetricsSquared& Metrics, const PCGExDataBlending::FMetadataBlender* InBlender, const int32 Offset) const
{
	const FPCGPoint& Start = SubPoints[0];
	const int32 LastIndex = SubPoints.Num() - 1;
	const FPCGPoint& End = SubPoints[LastIndex];
	BlendSubPoints(PCGEx::FPointRef(Start, Offset), PCGEx::FPointRef(End, Offset + LastIndex), SubPoints, Metrics, InBlender);
}

void UPCGExSubPointsBlendOperation::Write()
{
	if (InternalBlender) { InternalBlender->Write(); }
	Super::Write();
}

void UPCGExSubPointsBlendOperation::Cleanup()
{
	PCGEX_DELETE(InternalBlender)
	Super::Cleanup();
}

PCGExDataBlending::FMetadataBlender* UPCGExSubPointsBlendOperation::CreateBlender(
	PCGExData::FPointIO& InPrimaryIO, const PCGExData::FPointIO& InSecondaryIO, const bool bSecondaryIn)
{
	BlendingSettings.DefaultBlending = GetDefaultBlending();
	PCGExDataBlending::FMetadataBlender* NewBlender = new PCGExDataBlending::FMetadataBlender(&BlendingSettings);
	NewBlender->PrepareForData(InPrimaryIO, InSecondaryIO, bSecondaryIn);

	return NewBlender;
}
