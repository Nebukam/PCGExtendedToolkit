// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendOperation.h"

#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExMetadataBlender.h"

EPCGExDataBlendingType UPCGExSubPointsBlendOperation::GetDefaultBlending()
{
	return EPCGExDataBlendingType::Lerp;
}

void UPCGExSubPointsBlendOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	const UPCGExSubPointsBlendOperation* TypedOther = Cast<UPCGExSubPointsBlendOperation>(Other);
	if (TypedOther)
	{
		BlendingDetails = TypedOther->BlendingDetails;
	}
}

void UPCGExSubPointsBlendOperation::PrepareForData(PCGExData::FFacade* InPrimaryFacade)
{
	Super::PrepareForData(InPrimaryFacade);
	PrepareForData(InPrimaryFacade, InPrimaryFacade, PCGExData::ESource::In);
}

void UPCGExSubPointsBlendOperation::PrepareForData(
	PCGExData::FFacade* InPrimaryFacade,
	PCGExData::FFacade* InSecondaryFacade,
	const PCGExData::ESource SecondarySource)
{
	PCGEX_DELETE(InternalBlender)
	InternalBlender = CreateBlender(InPrimaryFacade, InSecondaryFacade, SecondarySource);
}

void UPCGExSubPointsBlendOperation::ProcessSubPoints(
	const PCGExData::FPointRef& Start,
	const PCGExData::FPointRef& End,
	const TArrayView<FPCGPoint>& SubPoints,
	const PCGExMath::FPathMetricsSquared& Metrics) const
{
	BlendSubPoints(Start, End, SubPoints, Metrics, InternalBlender);
}

void UPCGExSubPointsBlendOperation::ProcessSubPoints(
	const TArrayView<FPCGPoint>& SubPoints,
	const PCGExMath::FPathMetricsSquared& Metrics,
	const int32 Offset) const
{
	BlendSubPoints(SubPoints, Metrics, InternalBlender, Offset);
}

void UPCGExSubPointsBlendOperation::ProcessSubPoints(
	const TArrayView<FPCGPoint>& SubPoints,
	const PCGExMath::FPathMetricsSquared& PathInfos,
	PCGExDataBlending::FMetadataBlender* InBlender,
	const int32 Offset) const
{
	BlendSubPoints(SubPoints, PathInfos, InBlender, Offset);
}

void UPCGExSubPointsBlendOperation::BlendSubPoints(
	const PCGExData::FPointRef& StartPoint,
	const PCGExData::FPointRef& EndPoint,
	const TArrayView<FPCGPoint>& SubPoints,
	const PCGExMath::FPathMetricsSquared& Metrics,
	PCGExDataBlending::FMetadataBlender* InBlender) const
{
}

void UPCGExSubPointsBlendOperation::BlendSubPoints(const TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathMetricsSquared& Metrics, PCGExDataBlending::FMetadataBlender* InBlender, const int32 Offset) const
{
	const FPCGPoint& Start = SubPoints[0];
	const int32 LastIndex = SubPoints.Num() - 1;
	const FPCGPoint& End = SubPoints[LastIndex];
	BlendSubPoints(PCGExData::FPointRef(Start, Offset), PCGExData::FPointRef(End, Offset + LastIndex), SubPoints, Metrics, InBlender);
}

void UPCGExSubPointsBlendOperation::Cleanup()
{
	PCGEX_DELETE(InternalBlender)
	Super::Cleanup();
}

PCGExDataBlending::FMetadataBlender* UPCGExSubPointsBlendOperation::CreateBlender(
	PCGExData::FFacade* InPrimaryFacade,
	PCGExData::FFacade* InSecondaryFacade,
	const PCGExData::ESource SecondarySource)
{
	BlendingDetails.DefaultBlending = GetDefaultBlending();
	PCGExDataBlending::FMetadataBlender* NewBlender = new PCGExDataBlending::FMetadataBlender(&BlendingDetails);
	NewBlender->PrepareForData(InPrimaryFacade, InSecondaryFacade, SecondarySource);

	return NewBlender;
}
