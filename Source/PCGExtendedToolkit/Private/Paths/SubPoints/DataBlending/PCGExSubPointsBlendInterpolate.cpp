// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

EPCGExDataBlendingType UPCGExSubPointsBlendInterpolate::GetDefaultBlending()
{
	return EPCGExDataBlendingType::Lerp;
}

void UPCGExSubPointsBlendInterpolate::ApplyOverrides()
{
	Super::ApplyOverrides();
	PCGEX_OVERRIDE_OP_PROPERTY(Weight, FName(TEXT("Blending/Weight")), EPCGMetadataTypes::Double);
}

void UPCGExSubPointsBlendInterpolate::BlendSubPoints(
	const PCGEx::FPointRef& StartPoint,
	const PCGEx::FPointRef& EndPoint,
	TArrayView<FPCGPoint>& SubPoints,
	const PCGExMath::FPathMetricsSquared& Metrics,
	const PCGExDataBlending::FMetadataBlender* InBlender) const
{
	const int32 NumPoints = SubPoints.Num();

	EPCGExBlendOver SafeBlendOver = BlendOver;
	if (BlendOver == EPCGExBlendOver::Distance && !Metrics.IsValid()) { SafeBlendOver = EPCGExBlendOver::Index; }

	TArray<double> Weights;
	TArray<FVector> Locations;

	Weights.SetNumUninitialized(NumPoints);
	Locations.SetNumUninitialized(NumPoints);

	if (SafeBlendOver == EPCGExBlendOver::Distance)
	{
		PCGExMath::FPathMetricsSquared PathMetrics = PCGExMath::FPathMetricsSquared(StartPoint.Point->Transform.GetLocation());
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
			Weights[i] = Weight;
		}
	}

	InBlender->BlendRangeFromTo(StartPoint, EndPoint, StartPoint.Index, Weights);

	// Restore pre-blend position
	for (int i = 0; i < NumPoints; i++) { SubPoints[i].Transform.SetLocation(Locations[i]); }
}

PCGExDataBlending::FMetadataBlender* UPCGExSubPointsBlendInterpolate::CreateBlender(
	PCGExData::FPointIO& InPrimaryIO,
	const PCGExData::FPointIO& InSecondaryIO,
	const PCGExData::ESource SecondarySource)
{
	PCGExDataBlending::FMetadataBlender* NewBlender = new PCGExDataBlending::FMetadataBlender(&BlendingSettings);
	NewBlender->PrepareForData(InPrimaryIO, InSecondaryIO, SecondarySource);

	return NewBlender;
}
