// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/SubPoints/Orient/PCGExSubPointsOrientOperation.h"

#include "Data/PCGExPointIO.h"

void UPCGExSubPointsOrientOperation::PrepareForData(PCGExData::FPointIO& InPointIO)
{
	Super::PrepareForData(InPointIO);
}

void UPCGExSubPointsOrientOperation::ProcessSubPoints(const PCGEx::FPointRef& Start, const PCGEx::FPointRef& End, TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathMetrics& Metrics) const
{
	const int32 NumPointsMinusOne = SubPoints.Num() - 1;
	Orient(SubPoints[0], *Start.Point, SubPoints[1]);
	for (int i = 1; i < NumPointsMinusOne; i++) { Orient(SubPoints[i], SubPoints[i - 1], SubPoints[i + 1]); }
	Orient(SubPoints.Last(), SubPoints[NumPointsMinusOne - 1], *End.Point);
}

void UPCGExSubPointsOrientOperation::Orient(FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const
{
}
