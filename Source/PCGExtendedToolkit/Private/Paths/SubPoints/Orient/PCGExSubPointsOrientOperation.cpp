// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/SubPoints/Orient/PCGExSubPointsOrientOperation.h"

#include "Data/PCGExPointIO.h"

void UPCGExSubPointsOrientOperation::PrepareForData(PCGExData::FPointIO& InPointIO)
{
	Super::PrepareForData(InPointIO);
}

void UPCGExSubPointsOrientOperation::ProcessSubPoints(const PCGEx::FPointRef& Start, const PCGEx::FPointRef& End, TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathMetricsSquared& Metrics) const
{
	const int32 NumPoints = SubPoints.Num();
	const int32 NumPointsMinusOne = NumPoints - 1;

	if (bClosedPath)
	{
		for (int i = 0; i < SubPoints.Num(); i++)
		{
			Orient(
				SubPoints[i],
				SubPoints[PCGExMath::Tile(i - 1, 0, NumPointsMinusOne)],
				SubPoints[PCGExMath::Tile(i + 1, 0, NumPointsMinusOne)]);
		}
	}
	else
	{
		Orient(SubPoints[0], *Start.Point, SubPoints[1]);
		for (int i = 1; i < NumPointsMinusOne; i++) { Orient(SubPoints[i], SubPoints[i - 1], SubPoints[i + 1]); }
		Orient(SubPoints.Last(), SubPoints[NumPointsMinusOne - 1], *End.Point);
	}
}

void UPCGExSubPointsOrientOperation::Orient(FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const
{
}
