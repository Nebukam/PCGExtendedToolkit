// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInheritStart.h"

#include "Data/Blending/PCGExMetadataBlender.h"
#include "Editor/Experimental/EditorInteractiveToolsFramework/Public/Behaviors/2DViewportBehaviorTargets.h"
#include "Editor/Experimental/EditorInteractiveToolsFramework/Public/Behaviors/2DViewportBehaviorTargets.h"

void FPCGExSubPointsBlendInheritStart::BlendSubPoints(
	const PCGExData::FConstPoint& From, const PCGExData::FConstPoint& To,
	const TArrayView<FPCGPoint>& SubPoints, const PCGExPaths::FPathMetrics& Metrics, const int32 StartIndex) const
{
	const int32 NumPoints = SubPoints.Num();
	TArray<double> Weights;
	TArray<FVector> Locations;

	Weights.Reserve(NumPoints);
	Locations.Reserve(NumPoints);

	for (const FPCGPoint& Point : SubPoints)
	{
		Locations.Add(Point.Transform.GetLocation());
		Weights.Add(1);
	}

	InBlender->BlendRangeFromTo(From, To, StartIndex < 0 ? From.Index : StartIndex, Weights);

	// Restore pre-blend position
	for (int i = 0; i < NumPoints; i++) { SubPoints[i].Transform.SetLocation(Locations[i]); }
}

TSharedPtr<FPCGExSubPointsBlendOperation> UPCGExSubPointsBlendInheritStart::CreateOperation() const
{
	PCGEX_CREATE_SUBPOINTBLEND_OPERATION(InheritStart)
	return NewOperation;
}
