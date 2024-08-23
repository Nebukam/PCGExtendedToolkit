// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/Orient/PCGExOrientAverage.h"

#include "Geometry/PCGExGeo.h"

FTransform UPCGExOrientAverage::ComputeOrientation(const PCGExData::FPointRef& Point, const PCGExData::FPointRef& Previous, const PCGExData::FPointRef& Next, const double DirectionMultiplier) const
{
	const FVector PrevPos = Previous.Point->Transform.GetLocation();
	const FVector NextPos = Next.Point->Transform.GetLocation();
	FTransform OutT = Point.Point->Transform;
	OutT.SetRotation(
		PCGExMath::MakeDirection(
			OrientAxis, PCGExGeo::FApex(PrevPos, NextPos, OutT.GetLocation()).Direction * DirectionMultiplier,
			PCGExMath::GetDirection(UpAxis)));
	return OutT;
}
