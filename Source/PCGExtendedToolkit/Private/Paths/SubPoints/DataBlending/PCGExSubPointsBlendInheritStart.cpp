// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInheritStart.h"

#include "Data/Blending/PCGExMetadataBlender.h"

void UPCGExSubPointsBlendInheritStart::BlendSubPoints(
	const PCGEx::FPointRef& StartPoint,
	const PCGEx::FPointRef& EndPoint,
	TArrayView<FPCGPoint>& SubPoints,
	const PCGExMath::FPathMetricsSquared& Metrics,
	const PCGExDataBlending::FMetadataBlender* InBlender) const
{
	const int32 NumPoints = SubPoints.Num();
	TArray<double> Weights;
	TArray<FVector> Locations;

	Weights.Reserve(NumPoints);
	Locations.Reserve(NumPoints);

	for (const FPCGPoint& Point : SubPoints)
	{
		Locations.Add(Point.Transform.GetLocation());
		Weights.Add(0);
	}

	InBlender->BlendRangeFromTo(StartPoint, EndPoint, StartPoint.Index, Weights);

	// Restore pre-blend position
	for (int i = 0; i < NumPoints; i++) { SubPoints[i].Transform.SetLocation(Locations[i]); }
}
