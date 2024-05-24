// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

EPCGExDataBlendingType UPCGExSubPointsBlendInterpolate::GetDefaultBlending()
{
	return EPCGExDataBlendingType::Average;
}

void UPCGExSubPointsBlendInterpolate::ApplyOverrides()
{
	Super::ApplyOverrides();
	PCGEX_OVERRIDE_OP_PROPERTY(Alpha, FName(TEXT("Blending/Alpha")), EPCGMetadataTypes::Double);
}

void UPCGExSubPointsBlendInterpolate::BlendSubPoints(
	const PCGEx::FPointRef& StartPoint,
	const PCGEx::FPointRef& EndPoint,
	TArrayView<FPCGPoint>& SubPoints,
	const PCGExMath::FPathMetricsSquared& Metrics,
	const PCGExDataBlending::FMetadataBlender* InBlender) const
{
	const int32 NumPoints = SubPoints.Num();

	EPCGExPathBlendOver SafeBlendOver = BlendOver;
	if (BlendOver == EPCGExPathBlendOver::Distance && !Metrics.IsValid()) { SafeBlendOver = EPCGExPathBlendOver::Index; }

	TArray<double> Alphas;
	TArray<FVector> Locations;

	Alphas.Reserve(NumPoints);
	Locations.Reserve(NumPoints);

	if (SafeBlendOver == EPCGExPathBlendOver::Distance)
	{
		PCGExMath::FPathMetricsSquared PathMetrics = PCGExMath::FPathMetricsSquared(StartPoint.Point->Transform.GetLocation());
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

	InBlender->BlendRangeOnce(StartPoint, EndPoint, StartPoint.Index, NumPoints, Alphas);

	// Restore pre-blend position
	for (int i = 0; i < NumPoints; i++) { SubPoints[i].Transform.SetLocation(Locations[i]); }
}

PCGExDataBlending::FMetadataBlender* UPCGExSubPointsBlendInterpolate::CreateBlender(PCGExData::FPointIO& InPrimaryIO, const PCGExData::FPointIO& InSecondaryIO, const bool bSecondaryIn)
{
	PCGExDataBlending::FMetadataBlender* NewBlender = new PCGExDataBlending::FMetadataBlender(&BlendingSettings);
	NewBlender->PrepareForData(InPrimaryIO, InSecondaryIO, bSecondaryIn);

	return NewBlender;
}
