// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Splines/SubPoints/PCGExSubPointsOperation.h"

#include "..\..\..\Public\Data\PCGExPointIO.h"


void UPCGExSubPointsOperation::PrepareForData(FPCGExPointIO& InData, FPCGAttributeAccessorKeysPoints* InPrimaryKeys)
{
}

void UPCGExSubPointsOperation::ProcessPoints(UPCGPointData* InData) const
{
	TArray<FPCGPoint>& Points = InData->GetMutablePoints();
	TArrayView<FPCGPoint> Path = MakeArrayView(Points.GetData(), Points.Num());
	const PCGEx::FPointRef StartPoint = PCGEx::FPointRef(Points[0], 0);
	const PCGEx::FPointRef EndPoint = PCGEx::FPointRef(Points.Last(), Points.Num() - 1);
	ProcessSubPoints(StartPoint, EndPoint, Path, PCGExMath::FPathInfos());
}

void UPCGExSubPointsOperation::ProcessSubPoints(const PCGEx::FPointRef& Start, const PCGEx::FPointRef& End, TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathInfos& PathInfos) const
{
}

void UPCGExSubPointsOperation::ProcessSubPoints(TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathInfos& PathInfos) const
{
	const FPCGPoint& Start = SubPoints[0];
	const int32 LastIndex = SubPoints.Num()-1;
	const FPCGPoint& End = SubPoints[LastIndex];
	ProcessSubPoints(PCGEx::FPointRef(Start, 0), PCGEx::FPointRef(End, LastIndex), SubPoints, PathInfos);
}

