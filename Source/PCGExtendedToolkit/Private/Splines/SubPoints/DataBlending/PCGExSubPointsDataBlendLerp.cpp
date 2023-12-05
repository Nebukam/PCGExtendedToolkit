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

	PCGExDataBlending::FPropertiesBlender LocalPropertiesBlender = PCGExDataBlending::FPropertiesBlender(PropertiesBlender);

	EPCGExPathBlendOver SafeBlendOver = BlendOver;
	if (BlendOver == EPCGExPathBlendOver::Distance && !PathInfos.IsValid()) { SafeBlendOver = EPCGExPathBlendOver::Index; }

	if (SafeBlendOver == EPCGExPathBlendOver::Distance)
	{
		FVector PreviousLocation = StartPoint.Transform.GetLocation();
		PCGExMath::FPathInfos CurrentPathInfos = PCGExMath::FPathInfos(PreviousLocation);

		for (int i = 0; i < NumPoints; i++)
		{
			FPCGPoint& Point = SubPoints[i];

			const FVector Location = Point.Transform.GetLocation();
			CurrentPathInfos.Add(Location);

			const double Lerp = PathInfos.GetTime(CurrentPathInfos.Length);

			LocalPropertiesBlender.BlendSingle(StartPoint, EndPoint, Point, Lerp);
			InBlender->Blend(StartKey, EndKey, Point.MetadataEntry, Lerp);

			Point.Transform.SetLocation(Location); // !

			PreviousLocation = Location;
		}
	}
	else if (SafeBlendOver == EPCGExPathBlendOver::Index)
	{
		for (int i = 0; i < NumPoints; i++)
		{
			FPCGPoint& Point = SubPoints[i];
			const FVector Location = Point.Transform.GetLocation();

			const double Lerp = static_cast<double>(i + 1) / static_cast<double>(NumPoints);

			LocalPropertiesBlender.BlendSingle(StartPoint, EndPoint, Point, Lerp);
			InBlender->Blend(StartKey, EndKey, Point.MetadataEntry, Lerp);

			Point.Transform.SetLocation(Location); // !
		}
	}
	else if (SafeBlendOver == EPCGExPathBlendOver::Fixed)
	{
		for (int i = 0; i < NumPoints; i++)
		{
			FPCGPoint& Point = SubPoints[i];
			const FVector Location = Point.Transform.GetLocation();

			LocalPropertiesBlender.BlendSingle(StartPoint, EndPoint, Point, Alpha);
			InBlender->Blend(StartKey, EndKey, Point.MetadataEntry, Alpha);

			Point.Transform.SetLocation(Location); // !
		}
	}
}
