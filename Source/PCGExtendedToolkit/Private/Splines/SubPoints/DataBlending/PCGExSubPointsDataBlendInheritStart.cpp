// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Splines/SubPoints/DataBlending/PCGExSubPointsDataBlendInheritStart.h"

#include "Data/Blending/PCGExMetadataBlender.h"

void UPCGExSubPointsDataBlendInheritStart::BlendSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathInfos& PathInfos, const UPCGExMetadataBlender* InBlender) const
{
	const int32 NumPoints = SubPoints.Num();
	for (int i = 0; i < NumPoints; i++)
	{
		FPCGPoint& Point = SubPoints[i];
		const FVector Location = Point.Transform.GetLocation();

		PCGExMath::Copy(StartPoint, Point);
		InBlender->DoOperations(StartPoint.MetadataEntry, StartPoint.MetadataEntry, Point.MetadataEntry);

		Point.Transform.SetLocation(Location); // !
	}
}
