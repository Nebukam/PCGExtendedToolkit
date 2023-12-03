// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Splines/SubPoints/DataBlending/PCGExSubPointsDataBlendLerp.h"

void UPCGExSubPointsDataBlendLerp::ProcessSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const double PathLength) const
{
	const int32 NumPoints = SubPoints.Num();

	const PCGMetadataEntryKey StartKey = StartPoint.MetadataEntry;
	const PCGMetadataAttributeKey EndKey = EndPoint.MetadataEntry;

	if (LerpBase == EPCGExPathLerpBase::Distance)
	{
		FVector PreviousLocation = StartPoint.Transform.GetLocation();

		const double TotalDistance = FVector::DistSquared(PreviousLocation, EndPoint.Transform.GetLocation());
		double Distance = 0;

		for (int i = 0; i < NumPoints; i++)
		{
			FPCGPoint& Point = SubPoints[i];

			const FVector Location = Point.Transform.GetLocation();
			Distance += FVector::DistSquared(Location, PreviousLocation);

			const double Lerp = Distance / TotalDistance;
			PCGExMath::Lerp(StartPoint, EndPoint, Point, Lerp);
			AttributeMap->SetLerp(StartKey, EndKey, Point.MetadataEntry, Lerp);

			Point.Transform.SetLocation(Location); // !

			PreviousLocation = Location;
		}
	}
	else if (LerpBase == EPCGExPathLerpBase::Index)
	{
		for (int i = 0; i < NumPoints; i++)
		{
			FPCGPoint& Point = SubPoints[i];
			const FVector Location = Point.Transform.GetLocation();

			const double Lerp = (i + 1) / NumPoints;
			PCGExMath::Lerp(StartPoint, EndPoint, Point, Lerp);
			AttributeMap->SetLerp(StartKey, EndKey, Point.MetadataEntry, Lerp);

			Point.Transform.SetLocation(Location); // !
		}
	}
	else if (LerpBase == EPCGExPathLerpBase::Fixed)
	{
		for (int i = 0; i < NumPoints; i++)
		{
			FPCGPoint& Point = SubPoints[i];
			const FVector Location = Point.Transform.GetLocation();

			PCGExMath::Lerp(StartPoint, EndPoint, Point, Alpha);
			AttributeMap->SetLerp(StartKey, EndKey, Point.MetadataEntry, Alpha);

			Point.Transform.SetLocation(Location); // !
		}
	}
}
