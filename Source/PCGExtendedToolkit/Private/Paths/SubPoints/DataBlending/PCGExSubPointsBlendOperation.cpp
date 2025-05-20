// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendOperation.h"

#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "Editor/Experimental/EditorInteractiveToolsFramework/Public/Behaviors/2DViewportBehaviorTargets.h"
#include "Editor/Experimental/EditorInteractiveToolsFramework/Public/Behaviors/2DViewportBehaviorTargets.h"

bool FPCGExSubPointsBlendOperation::PrepareForData(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InTargetFacade, const TSet<FName>* IgnoreAttributeSet)
{
	return PrepareForData(InContext, InTargetFacade, InTargetFacade, PCGExData::EIOSide::In, IgnoreAttributeSet);
}

bool FPCGExSubPointsBlendOperation::PrepareForData(
	FPCGExContext* InContext,
	const TSharedPtr<PCGExData::FFacade>& InTargetFacade,
	const TSharedPtr<PCGExData::FFacade>& InSourceFacade, const PCGExData::EIOSide InSourceSide,
	const TSet<FName>* IgnoreAttributeSet)
{
	if (!FPCGExSubPointsOperation::PrepareForData(InContext, InTargetFacade, IgnoreAttributeSet)) { return false; }

	if (bPreserveTransform) { bPreservePosition = bPreserveRotation = bPreserveScale = true; }

	BlendingDetails = BlendFactory->BlendingDetails;

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

	MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>();

	MetadataBlender->SetTargetData(InTargetFacade);
	MetadataBlender->SetSourceData(InSourceFacade, InSourceSide);

	return MetadataBlender->Init(InContext, BlendingDetails, IgnoreAttributeSet);
}

void FPCGExSubPointsBlendOperation::ProcessSubPoints(
	const PCGExData::FConstPoint& From, const PCGExData::FConstPoint& To,
	const TArrayView<FPCGPoint>& SubPoints, const PCGExPaths::FPathMetrics& Metrics, const int32 StartIndex) const
{
	BlendSubPoints(From, To, SubPoints, Metrics, StartIndex);
}

void FPCGExSubPointsBlendOperation::BlendSubPoints(
	const PCGExData::FConstPoint& From, const PCGExData::FConstPoint& To,
	const TArrayView<FPCGPoint>& SubPoints, const PCGExPaths::FPathMetrics& Metrics, const int32 StartIndex) const
{
}

void FPCGExSubPointsBlendOperation::BlendSubPoints(TArray<FPCGPoint>& SubPoints, const PCGExPaths::FPathMetrics& Metrics) const
{
	const FPCGPoint& Start = SubPoints[0];
	const int32 LastIndex = SubPoints.Num() - 1;
	const FPCGPoint& End = SubPoints[LastIndex];
	BlendSubPoints(PCGExData::FConstPoint(Start, 0), PCGExData::FConstPoint(End, LastIndex), SubPoints, Metrics, 0);
}

void UPCGExSubPointsBlendInstancedFactory::CopySettingsFrom(const UPCGExInstancedFactory* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExSubPointsBlendInstancedFactory* TypedOther = Cast<UPCGExSubPointsBlendInstancedFactory>(Other))
	{
		BlendingDetails = TypedOther->BlendingDetails;
	}
}

TSharedPtr<FPCGExSubPointsBlendOperation> UPCGExSubPointsBlendInstancedFactory::CreateOperation() const
{
	return nullptr;
}
