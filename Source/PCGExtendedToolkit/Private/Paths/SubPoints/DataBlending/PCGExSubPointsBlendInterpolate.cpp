// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#include "Editor/Experimental/EditorInteractiveToolsFramework/Public/Behaviors/2DViewportBehaviorTargets.h"
#include "Editor/Experimental/EditorInteractiveToolsFramework/Public/Behaviors/2DViewportBehaviorTargets.h"

void FPCGExSubPointsBlendInterpolate::BlendSubPoints(
	const PCGExData::FConstPoint& From, const PCGExData::FConstPoint& To,
	const TArrayView<FPCGPoint>& SubPoints, const PCGExPaths::FPathMetrics& Metrics, const int32 StartIndex) const
{
	const int32 NumPoints = SubPoints.Num();

	EPCGExBlendOver SafeBlendOver = InterpolateFactory->BlendOver;
	if (InterpolateFactory->BlendOver == EPCGExBlendOver::Distance && !Metrics.IsValid()) { SafeBlendOver = EPCGExBlendOver::Index; }

	TArray<double> Weights;
	TArray<FVector> Locations;

	Weights.SetNumUninitialized(NumPoints);
	Locations.SetNumUninitialized(NumPoints);

	if (SafeBlendOver == EPCGExBlendOver::Distance)
	{
		PCGExPaths::FPathMetrics PathMetrics = PCGExPaths::FPathMetrics(From.Point->Transform.GetLocation());
		for (int i = 0; i < NumPoints; i++)
		{
			const FVector Location = SubPoints[i].Transform.GetLocation();
			Locations[i] = Location;
			Weights[i] = Metrics.GetTime(PathMetrics.Add(Location));
		}
	}
	else if (SafeBlendOver == EPCGExBlendOver::Index)
	{
		for (int i = 0; i < NumPoints; i++)
		{
			Locations[i] = SubPoints[i].Transform.GetLocation();
			Weights[i] = static_cast<double>(i) / NumPoints;
		}
	}
	else if (SafeBlendOver == EPCGExBlendOver::Fixed)
	{
		for (int i = 0; i < NumPoints; i++)
		{
			Locations[i] = SubPoints[i].Transform.GetLocation();
			Weights[i] = Lerp;
		}
	}

	InBlender->BlendRangeFromTo(From, To, StartIndex < 0 ? From.Index : StartIndex, Weights);

	// Restore pre-blend position
	for (int i = 0; i < NumPoints; i++) { SubPoints[i].Transform.SetLocation(Locations[i]); }
}

void UPCGExSubPointsBlendInterpolate::CopySettingsFrom(const UPCGExInstancedFactory* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExSubPointsBlendInterpolate* TypedOther = Cast<UPCGExSubPointsBlendInterpolate>(Other))
	{
		BlendOver = TypedOther->BlendOver;
		Lerp = TypedOther->Lerp;
	}
}

TSharedPtr<FPCGExSubPointsBlendOperation> UPCGExSubPointsBlendInterpolate::CreateOperation() const
{
	PCGEX_CREATE_SUBPOINTBLEND_OPERATION(Interpolate)
	NewOperation->InterpolateFactory = this;
	return NewOperation;
}
