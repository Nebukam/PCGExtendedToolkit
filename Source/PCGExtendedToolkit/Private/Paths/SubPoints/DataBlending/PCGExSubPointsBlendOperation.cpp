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
	if (const UPCGExSubPointsBlendOperation* TypedOther = Cast<UPCGExSubPointsBlendOperation>(Other))
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
	const PCGExData::FPointRef& From,
	const PCGExData::FPointRef& To,
	const TArrayView<FPCGPoint>& SubPoints,
	const PCGExPaths::FPathMetrics& Metrics,
	const int32 StartIndex) const
{
	BlendSubPoints(From, To, SubPoints, Metrics, InternalBlender, StartIndex);
}

void UPCGExSubPointsBlendOperation::BlendSubPoints(
	const PCGExData::FPointRef& From,
	const PCGExData::FPointRef& To,
	const TArrayView<FPCGPoint>& SubPoints,
	const PCGExPaths::FPathMetrics& Metrics,
	PCGExDataBlending::FMetadataBlender* InBlender,
	const int32 StartIndex) const
{
}

void UPCGExSubPointsBlendOperation::BlendSubPoints(TArray<FPCGPoint>& SubPoints, const PCGExPaths::FPathMetrics& Metrics, PCGExDataBlending::FMetadataBlender* InBlender) const
{
	const FPCGPoint& Start = SubPoints[0];
	const int32 LastIndex = SubPoints.Num() - 1;
	const FPCGPoint& End = SubPoints[LastIndex];
	BlendSubPoints(PCGExData::FPointRef(Start, 0), PCGExData::FPointRef(End, LastIndex), SubPoints, Metrics, InBlender, 0);
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
