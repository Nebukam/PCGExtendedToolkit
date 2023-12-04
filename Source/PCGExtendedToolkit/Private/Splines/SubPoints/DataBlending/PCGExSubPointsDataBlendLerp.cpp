// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Splines/SubPoints/DataBlending/PCGExSubPointsDataBlendLerp.h"

#include "Data/PCGExPointIO.h"

EPCGExDataBlendingType UPCGExSubPointsDataBlendLerp::GetDefaultBlending()
{
	return EPCGExDataBlendingType::Weight;
}

void UPCGExSubPointsDataBlendLerp::BlendSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathInfos& PathInfos, const UPCGExMetadataBlender* InBlender) const
{
	const int32 NumPoints = SubPoints.Num();

	const PCGMetadataEntryKey StartKey = StartPoint.MetadataEntry;
	const PCGMetadataAttributeKey EndKey = EndPoint.MetadataEntry;

	EPCGExPathLerpBase SafeLerpBase = LerpBase;
	if (LerpBase == EPCGExPathLerpBase::Distance && !PathInfos.IsValid()) { SafeLerpBase = EPCGExPathLerpBase::Index; }

	if (SafeLerpBase == EPCGExPathLerpBase::Distance)
	{
		FVector PreviousLocation = StartPoint.Transform.GetLocation();
		PCGExMath::FPathInfos CurrentPathInfos = PCGExMath::FPathInfos(PreviousLocation);

		for (int i = 0; i < NumPoints; i++)
		{
			FPCGPoint& Point = SubPoints[i];

			const FVector Location = Point.Transform.GetLocation();
			CurrentPathInfos.Add(Location);

			const double Lerp = PathInfos.GetTime(CurrentPathInfos.Length);
			PCGExMath::Lerp(StartPoint, EndPoint, Point, Lerp);
			InBlender->DoOperations(StartKey, EndKey, Point.MetadataEntry, Lerp);

			Point.Transform.SetLocation(Location); // !

			PreviousLocation = Location;
		}
	}
	else if (SafeLerpBase == EPCGExPathLerpBase::Index)
	{
		for (int i = 0; i < NumPoints; i++)
		{
			FPCGPoint& Point = SubPoints[i];
			const FVector Location = Point.Transform.GetLocation();

			const double Lerp = (i + 1) / NumPoints;
			PCGExMath::Lerp(StartPoint, EndPoint, Point, Lerp);
			InBlender->DoOperations(StartKey, EndKey, Point.MetadataEntry, Lerp);

			Point.Transform.SetLocation(Location); // !
		}
	}
	else if (SafeLerpBase == EPCGExPathLerpBase::Fixed)
	{
		for (int i = 0; i < NumPoints; i++)
		{
			FPCGPoint& Point = SubPoints[i];
			const FVector Location = Point.Transform.GetLocation();

			PCGExMath::Lerp(StartPoint, EndPoint, Point, Alpha);
			InBlender->DoOperations(StartKey, EndKey, Point.MetadataEntry, Alpha);

			Point.Transform.SetLocation(Location); // !
		}
	}
}
