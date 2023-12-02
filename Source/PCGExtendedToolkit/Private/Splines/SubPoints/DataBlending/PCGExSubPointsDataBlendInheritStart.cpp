// Fill out your copyright notice in the Description page of Project Settings.


#include "Splines/SubPoints/DataBlending/PCGExSubPointsDataBlendInheritStart.h"

void UPCGExSubPointsDataBlendInheritStart::ProcessSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const double PathLength) const
{	
	const int32 NumPoints = SubPoints.Num();
	for (int i = 0; i < NumPoints; i++)
	{
		FPCGPoint& Point = SubPoints[i];		
		const FVector Location = Point.Transform.GetLocation();
		
		PCGExMath::Copy(StartPoint, Point);
		AttributeMap->SetCopy(StartPoint.MetadataEntry, Point.MetadataEntry);
		
		Point.Transform.SetLocation(Location); // !
	}
}
