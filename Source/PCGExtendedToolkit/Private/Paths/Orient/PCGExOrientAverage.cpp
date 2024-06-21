// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/Orient/PCGExOrientAverage.h"

void UPCGExOrientAverage::Orient(PCGEx::FPointRef& Point, const PCGEx::FPointRef& Previous, const PCGEx::FPointRef& Next, const double Factor) const
{
	const FVector PrevPos = Previous.Point->Transform.GetLocation();
	const FVector NextPos = Next.Point->Transform.GetLocation();
	FPCGPoint& MPoint = Point.MutablePoint();
	MPoint.Transform.SetRotation(
		PCGExMath::MakeDirection(
			OrientAxis, PCGExMath::FApex(PrevPos, NextPos, MPoint.Transform.GetLocation()).Direction * Factor,
			PCGExMath::GetDirection(UpAxis)));
}
