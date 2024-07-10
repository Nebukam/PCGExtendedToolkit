// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/Orient/PCGExOrientAverage.h"

FTransform UPCGExOrientAverage::ComputeOrientation(const PCGEx::FPointRef& Point, const PCGEx::FPointRef& Previous, const PCGEx::FPointRef& Next, const double DirectionMultiplier) const
{
	const FVector PrevPos = Previous.Point->Transform.GetLocation();
	const FVector NextPos = Next.Point->Transform.GetLocation();
	FTransform OutT = Point.Point->Transform;
	OutT.SetRotation(
		PCGExMath::MakeDirection(
			OrientAxis, PCGExMath::FApex(PrevPos, NextPos, OutT.GetLocation()).Direction * DirectionMultiplier,
			PCGExMath::GetDirection(UpAxis)));
	return OutT;
}
