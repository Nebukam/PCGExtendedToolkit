// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Splines/SubPoints/Orient/PCGExSubPointsOrientAverage.h"

void UPCGExSubPointsOrientAverage::ProcessSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const double PathLength) const
{/*
	const int32 NumPoints = SubPoints.Num();

	const PCGMetadataEntryKey StartKey = StartPoint.MetadataEntry;
	const PCGMetadataAttributeKey EndKey = EndPoint.MetadataEntry;

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
		InBlender->DoOperations(StartKey, EndKey, Point.MetadataEntry, Lerp);

		Point.Transform.SetLocation(Location); // !

		PreviousLocation = Location;
	}
	*/
}
