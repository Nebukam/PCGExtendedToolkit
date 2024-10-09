// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Orient/PCGExOrientWeighted.h"

#include "Geometry/PCGExGeo.h"

void UPCGExOrientWeighted::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExOrientWeighted* TypedOther = Cast<UPCGExOrientWeighted>(Other))
	{
		bInverseWeight = TypedOther->bInverseWeight;
	}
}

FTransform UPCGExOrientWeighted::ComputeOrientation(const PCGExData::FPointRef& Point, const PCGExData::FPointRef& Previous, const PCGExData::FPointRef& Next, const double DirectionMultiplier) const
{
	FTransform OutT = Point.MutablePoint().Transform;
	const FVector Current = OutT.GetLocation();

	const FVector PrevPos = Previous.Point->Transform.GetLocation();
	const FVector NextPos = Next.Point->Transform.GetLocation();
	const FVector DirToPrev = (PrevPos - Current);
	const FVector DirToNext = (Current - NextPos);
	const double Weight = PCGExGeo::FApex(PrevPos, NextPos, Current).Alpha;

	OutT.SetRotation(
		PCGExMath::MakeDirection(
			OrientAxis,
			FMath::Lerp(DirToPrev, DirToNext, bInverseWeight ? 1 - Weight : Weight).GetSafeNormal() * DirectionMultiplier,
			PCGExMath::GetDirection(UpAxis)));

	return OutT;
}
