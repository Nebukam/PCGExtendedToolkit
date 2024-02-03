// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/SubPoints/Orient/PCGExSubPointsOrientAverage.h"

void UPCGExSubPointsOrientAverage::Orient(FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const
{
	const FVector Previous = PreviousPoint.Transform.GetLocation();
	const FVector Next = NextPoint.Transform.GetLocation();
	Point.Transform.SetRotation(
		PCGExMath::MakeDirection(
			OrientAxis, PCGExMath::FApex(Previous, Next, Point.Transform.GetLocation()).Direction,
			PCGExMath::GetDirection(UpAxis)));
}
