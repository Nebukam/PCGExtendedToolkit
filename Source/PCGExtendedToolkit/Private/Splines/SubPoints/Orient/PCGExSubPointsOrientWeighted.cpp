// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Splines/SubPoints/Orient/PCGExSubPointsOrientWeighted.h"

void UPCGExSubPointsOrientWeighted::ProcessSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathInfos& PathInfos) const
{
	const int32 NumPointsMinusOne = SubPoints.Num() - 1;
	if (bInverseWeight)
	{
		Orient(SubPoints[0], StartPoint, SubPoints[1]);
		for (int i = 1; i < NumPointsMinusOne; i++) { Orient(SubPoints[i], SubPoints[i - 1], SubPoints[i + 1]); }
		Orient(SubPoints.Last(), SubPoints[NumPointsMinusOne - 1], EndPoint);
	}
	else
	{
		OrientInvertedWeight(SubPoints[0], StartPoint, SubPoints[1]);
		for (int i = 1; i < NumPointsMinusOne; i++) { OrientInvertedWeight(SubPoints[i], SubPoints[i - 1], SubPoints[i + 1]); }
		OrientInvertedWeight(SubPoints.Last(), SubPoints[NumPointsMinusOne - 1], EndPoint);
	}
}

void UPCGExSubPointsOrientWeighted::Orient(FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const
{
	const FVector Current = Point.Transform.GetLocation();
	const FVector Previous = PreviousPoint.Transform.GetLocation();
	const FVector Next = NextPoint.Transform.GetLocation();
	const FVector DirToPrev = (Previous - Current);
	const FVector DirToNext = (Current - Next);
	const double Weight = PCGExMath::FApex(Previous, Next, Point.Transform.GetLocation()).Alpha;

	Point.Transform.SetRotation(
		PCGEx::MakeDirection(
			OrientAxis,
			FMath::Lerp(DirToPrev, DirToNext, 1 - Weight),
			PCGEx::GetDirection(UpAxis)));
}

void UPCGExSubPointsOrientWeighted::OrientInvertedWeight(FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const
{
	const FVector Current = Point.Transform.GetLocation();
	const FVector Previous = PreviousPoint.Transform.GetLocation();
	const FVector Next = NextPoint.Transform.GetLocation();
	const FVector DirToPrev = (Previous - Current);
	const FVector DirToNext = (Current - Next);
	const double Weight = PCGExMath::FApex(Previous, Next, Point.Transform.GetLocation()).Alpha;

	Point.Transform.SetRotation(
		PCGEx::MakeDirection(
			OrientAxis,
			FMath::Lerp(DirToPrev, DirToNext, Weight),
			PCGEx::GetDirection(UpAxis)));
}
