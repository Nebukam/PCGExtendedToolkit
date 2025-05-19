// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendOperation.h"

#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExMetadataBlender.h"


EPCGExDataBlendingType UPCGExSubPointsBlendOperation::GetDefaultBlending()
{
	return EPCGExDataBlendingType::Lerp;
}

void UPCGExSubPointsBlendOperation::CopySettingsFrom(const UPCGExInstancedFactory* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExSubPointsBlendOperation* TypedOther = Cast<UPCGExSubPointsBlendOperation>(Other))
	{
		BlendingDetails = TypedOther->BlendingDetails;
	}
}

void UPCGExSubPointsBlendOperation::PrepareForData(const TSharedPtr<PCGExData::FFacade>& InTargetFacade, const TSet<FName>* IgnoreAttributeSet)
{
	Super::PrepareForData(InTargetFacade, IgnoreAttributeSet);
	PrepareForData(InTargetFacade, InTargetFacade, PCGExData::EIOSide::In, IgnoreAttributeSet);
}

void UPCGExSubPointsBlendOperation::PrepareForData(
	const TSharedPtr<PCGExData::FFacade>& InTargetFacade,
	const TSharedPtr<PCGExData::FFacade>& InSourceFacade,
	const PCGExData::EIOSide InSourceSide,
	const TSet<FName>* IgnoreAttributeSet)
{
	if (bPreserveTransform) { bPreservePosition = bPreserveRotation = bPreserveScale = true; }

	if (bPreservePosition)
	{
		BlendingDetails.PropertiesOverrides.bOverridePosition = true;
		BlendingDetails.PropertiesOverrides.PositionBlending = EPCGExDataBlendingType::None;
	}

	if (bPreserveRotation)
	{
		BlendingDetails.PropertiesOverrides.bOverrideRotation = true;
		BlendingDetails.PropertiesOverrides.RotationBlending = EPCGExDataBlendingType::None;
	}

	if (bPreserveScale)
	{
		BlendingDetails.PropertiesOverrides.bOverrideScale = true;
		BlendingDetails.PropertiesOverrides.ScaleBlending = EPCGExDataBlendingType::None;
	}

	InternalBlender.Reset();
	InternalBlender = CreateBlender(
		InTargetFacade.ToSharedRef(), InSourceFacade.ToSharedRef(),
		InSourceSide, IgnoreAttributeSet);
}

void UPCGExSubPointsBlendOperation::ProcessSubPoints(
	const PCGExData::FConstPoint& From,
	const PCGExData::FConstPoint& To,
	const TArrayView<FPCGPoint>& SubPoints,
	const PCGExPaths::FPathMetrics& Metrics,
	const int32 StartIndex) const
{
	BlendSubPoints(From, To, SubPoints, Metrics, InternalBlender.Get(), StartIndex);
}

void UPCGExSubPointsBlendOperation::BlendSubPoints(
	const PCGExData::FConstPoint& From,
	const PCGExData::FConstPoint& To,
	const TArrayView<FPCGPoint>& SubPoints,
	const PCGExPaths::FPathMetrics& Metrics,
	PCGExDataBlending::FMetadataBlender* InBlender,
	const int32 StartIndex) const
{
}

void UPCGExSubPointsBlendOperation::BlendSubPoints(
	TArray<FPCGPoint>& SubPoints,
	const PCGExPaths::FPathMetrics& Metrics,
	PCGExDataBlending::FMetadataBlender* InBlender) const
{
	const FPCGPoint& Start = SubPoints[0];
	const int32 LastIndex = SubPoints.Num() - 1;
	const FPCGPoint& End = SubPoints[LastIndex];
	BlendSubPoints(PCGExData::FConstPoint(Start, 0), PCGExData::FConstPoint(End, LastIndex), SubPoints, Metrics, InBlender, 0);
}

void UPCGExSubPointsBlendOperation::Cleanup()
{
	InternalBlender.Reset();
	Super::Cleanup();
}

TSharedPtr<PCGExDataBlending::FMetadataBlender> UPCGExSubPointsBlendOperation::CreateBlender(
	const TSharedRef<PCGExData::FFacade>& InTargetFacade,
	const TSharedRef<PCGExData::FFacade>& InSourceFacade,
	const PCGExData::EIOSide InSourceSide,
	const TSet<FName>* IgnoreAttributeSet)
{
	BlendingDetails.DefaultBlending = GetDefaultBlending();
	PCGEX_MAKE_SHARED(NewBlender, PCGExDataBlending::FMetadataBlender, &BlendingDetails)
	NewBlender->PrepareForData(InTargetFacade, InSourceFacade, InSourceSide, true, IgnoreAttributeSet);

	return NewBlender;
}
