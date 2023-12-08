// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Splines/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#include "..\..\..\..\Public\Data\PCGExPointsIO.h"

EPCGExDataBlendingType UPCGExSubPointsBlendInterpolate::GetDefaultBlending()
{
	return EPCGExDataBlendingType::Weight;
}

void UPCGExSubPointsBlendInterpolate::BlendSubPoints(
	const PCGEx::FPointRef& StartPoint,
	const PCGEx::FPointRef& EndPoint,
	TArrayView<FPCGPoint>& SubPoints,
	const PCGExMath::FPathInfos& PathInfos,
	const PCGExDataBlending::FMetadataBlender* InBlender) const
{
	const int32 NumPoints = SubPoints.Num();
	PCGExDataBlending::FPropertiesBlender LocalPropertiesBlender = PCGExDataBlending::FPropertiesBlender(PropertiesBlender);

	EPCGExPathBlendOver SafeBlendOver = BlendOver;
	if (BlendOver == EPCGExPathBlendOver::Distance && !PathInfos.IsValid()) { SafeBlendOver = EPCGExPathBlendOver::Index; }

	TArray<double> Alphas;
	TArray<FVector> Locations;

	Alphas.Reserve(NumPoints);
	Locations.Reserve(NumPoints);

	if (SafeBlendOver == EPCGExPathBlendOver::Distance)
	{
		PCGExMath::FPathInfos CurrentPathInfos = PCGExMath::FPathInfos(StartPoint.Point->Transform.GetLocation());
		for (const FPCGPoint& Point : SubPoints)
		{
			const FVector Location = Point.Transform.GetLocation();
			CurrentPathInfos.Add(Location);
			Locations.Add(Location);
			Alphas.Add(PathInfos.GetTime(CurrentPathInfos.Length));
		}
	}
	else if (SafeBlendOver == EPCGExPathBlendOver::Index)
	{
		for (int i = 0; i < NumPoints; i++)
		{
			Locations.Add(SubPoints[i].Transform.GetLocation());
			Alphas.Add(static_cast<double>(i + 1) / static_cast<double>(NumPoints));
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

	LocalPropertiesBlender.BlendRangeOnce(*StartPoint.Point, *EndPoint.Point, SubPoints, Alphas);
	InBlender->BlendRangeOnce(StartPoint.Index, EndPoint.Index, StartPoint.Index, NumPoints, Alphas);

	// Restore pre-blend position
	for (int i = 0; i < NumPoints; i++) { SubPoints[i].Transform.SetLocation(Locations[i]); }
}
