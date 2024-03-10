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
	TArray<double> Alphas;
	TArray<FVector> Locations;

	Alphas.Reserve(NumPoints);
	Locations.Reserve(NumPoints);

	for (const FPCGPoint& Point : SubPoints)
	{
		Locations.Add(Point.Transform.GetLocation());
		Alphas.Add(0);
	}

	InBlender->BlendRangeOnce(StartPoint, EndPoint, StartPoint.Index, NumPoints, Alphas);

	// Restore pre-blend position
	for (int i = 0; i < NumPoints; i++) { SubPoints[i].Transform.SetLocation(Locations[i]); }
}
