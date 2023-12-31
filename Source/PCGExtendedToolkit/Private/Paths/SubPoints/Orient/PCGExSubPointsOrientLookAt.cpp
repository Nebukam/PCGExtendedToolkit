﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/SubPoints/Orient/PCGExSubPointsOrientLookAt.h"

void UPCGExSubPointsOrientLookAt::ProcessSubPoints(const PCGEx::FPointRef& Start, const PCGEx::FPointRef& End, TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathMetrics& Metrics) const
{
	const int32 NumPointsMinusOne = SubPoints.Num() - 1;

	switch (LookAt)
	{
	case EPCGExOrientLookAt::NextPoint:
		LookAtNext(SubPoints[0], *Start.Point, SubPoints[1]);
		for (int i = 1; i < NumPointsMinusOne; i++) { LookAtNext(SubPoints[i], SubPoints[i - 1], SubPoints[i + 1]); }
		LookAtNext(SubPoints.Last(), SubPoints[NumPointsMinusOne - 1], *End.Point);
		break;
	case EPCGExOrientLookAt::PreviousPoint:
		LookAtPrev(SubPoints[0], *Start.Point, SubPoints[1]);
		for (int i = 1; i < NumPointsMinusOne; i++) { LookAtPrev(SubPoints[i], SubPoints[i - 1], SubPoints[i + 1]); }
		LookAtPrev(SubPoints.Last(), SubPoints[NumPointsMinusOne - 1], *End.Point);
		break;
	case EPCGExOrientLookAt::Attribute:
		LookAtAttribute(SubPoints[0], *Start.Point, SubPoints[1]);
		for (int i = 1; i < NumPointsMinusOne; i++) { LookAtAttribute(SubPoints[i], SubPoints[i - 1], SubPoints[i + 1]); }
		LookAtAttribute(SubPoints.Last(), SubPoints[NumPointsMinusOne - 1], *End.Point);
		break;
	}
}

void UPCGExSubPointsOrientLookAt::LookAtNext(FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const
{
	Point.Transform.SetRotation(
		PCGEx::MakeDirection(
			OrientAxis,
			(Point.Transform.GetLocation() - NextPoint.Transform.GetLocation()),
			PCGEx::GetDirection(UpAxis)));
}

void UPCGExSubPointsOrientLookAt::LookAtPrev(FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const
{
	Point.Transform.SetRotation(
		PCGEx::MakeDirection(
			OrientAxis,
			(Point.Transform.GetLocation() - PreviousPoint.Transform.GetLocation()),
			PCGEx::GetDirection(UpAxis)));
}

void UPCGExSubPointsOrientLookAt::LookAtAttribute(FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const
{
}
