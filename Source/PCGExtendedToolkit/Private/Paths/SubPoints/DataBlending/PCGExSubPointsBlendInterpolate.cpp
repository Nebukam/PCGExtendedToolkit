// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

EPCGExDataBlendingType UPCGExSubPointsBlendInterpolate::GetDefaultBlending()
{
	return EPCGExDataBlendingType::Weight;
}

void UPCGExSubPointsBlendInterpolate::BlendSubPoints(
	const PCGEx::FPointRef& StartPoint,
	const PCGEx::FPointRef& EndPoint,
	TArrayView<FPCGPoint>& SubPoints,
	const PCGExMath::FPathMetrics& Metrics,
	const PCGExDataBlending::FMetadataBlender* InBlender) const
{
	const int32 NumPoints = SubPoints.Num();
	PCGExDataBlending::FPropertiesBlender LocalPropertiesBlender = PCGExDataBlending::FPropertiesBlender(PropertiesBlender);

	EPCGExPathBlendOver SafeBlendOver = BlendOver;
	if (BlendOver == EPCGExPathBlendOver::Distance && !Metrics.IsValid()) { SafeBlendOver = EPCGExPathBlendOver::Index; }

	TArray<double> Alphas;
	TArray<FVector> Locations;

	Alphas.Reserve(NumPoints);
	Locations.Reserve(NumPoints);

	if (SafeBlendOver == EPCGExPathBlendOver::Distance)
	{
		PCGExMath::FPathMetrics PathMetrics = PCGExMath::FPathMetrics(StartPoint.Point->Transform.GetLocation());
		for (const FPCGPoint& Point : SubPoints)
		{
			const FVector Location = Point.Transform.GetLocation();
			Locations.Add(Location);
			Alphas.Add(Metrics.GetTime(PathMetrics.Add(Location)));
		}
	}
	else if (SafeBlendOver == EPCGExPathBlendOver::Index)
	{
		for (int i = 0; i < NumPoints; i++)
		{
			Locations.Add(SubPoints[i].Transform.GetLocation());
			Alphas.Add(static_cast<double>(i + 1) / static_cast<double>(Metrics.Count));
		}
	}
	else if (SafeBlendOver == EPCGExPathBlendOver::Fixed)
	{
		for (const FPCGPoint& Point : SubPoints)
		{
			Locations.Add(Point.Transform.GetLocation());
			Alphas.Add(Alpha);
		}
	}

	/*
		FString str = TEXT("-> ");
		for (double Al : Alphas)
		{
			str += FString::Printf(TEXT("%f | "), Al);
		}
	
		str += FString::Printf(TEXT(" // %f"), Metrics.Length);
		str += FString::Printf(TEXT(" // %d"), Metrics.Count);
		
		UE_LOG(LogTemp, Warning, TEXT("%s"), *str);
	*/

	LocalPropertiesBlender.BlendRangeOnce(*StartPoint.Point, *EndPoint.Point, SubPoints, Alphas);
	InBlender->BlendRangeOnce(StartPoint.Index, EndPoint.Index, StartPoint.Index, NumPoints, Alphas);

	// Restore pre-blend position
	for (int i = 0; i < NumPoints; i++) { SubPoints[i].Transform.SetLocation(Locations[i]); }
}
