// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Splines/SubPoints/Orient/PCGExSubPointsOrientOperation.h"

#include "..\..\..\..\Public\Data\PCGExPointsIO.h"

void UPCGExSubPointsOrientOperation::PrepareForData(FPCGExPointIO& InData, FPCGAttributeAccessorKeysPoints* InPrimaryKeys)
{
	Super::PrepareForData(InData, InPrimaryKeys);
}

void UPCGExSubPointsOrientOperation::ProcessSubPoints(const PCGEx::FPointRef& Start, const PCGEx::FPointRef& End, TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathInfos& PathInfos) const
{
	const int32 NumPointsMinusOne = SubPoints.Num() - 1;
	Orient(SubPoints[0], *Start.Point, SubPoints[1]);
	for (int i = 1; i < NumPointsMinusOne; i++) { Orient(SubPoints[i], SubPoints[i - 1], SubPoints[i + 1]); }
	Orient(SubPoints.Last(), SubPoints[NumPointsMinusOne - 1], *End.Point);
}

void UPCGExSubPointsOrientOperation::Orient(FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const
{
}
