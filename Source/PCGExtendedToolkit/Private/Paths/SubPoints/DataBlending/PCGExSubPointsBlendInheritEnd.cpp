// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInheritEnd.h"
#include "Data/Blending/PCGExMetadataBlender.h"

void UPCGExSubPointsBlendInheritEnd::BlendSubPoints(
	const PCGExData::FPointRef& From,
	const PCGExData::FPointRef& To,
	const TArrayView<FPCGPoint>& SubPoints,
	const PCGExPaths::FPathMetrics& Metrics,
	PCGExDataBlending::FMetadataBlender* InBlender,
	const int32 StartIndex) const
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
