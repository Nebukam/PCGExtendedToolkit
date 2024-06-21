// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Orient/PCGExOrientWeighted.h"

void UPCGExOrientWeighted::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	const UPCGExOrientWeighted* TypedOther = Cast<UPCGExOrientWeighted>(Other);
	if (TypedOther)
	{
		bInverseWeight = TypedOther->bInverseWeight;
	}
}

void UPCGExOrientWeighted::Orient(PCGEx::FPointRef& Point, const PCGEx::FPointRef& Previous, const PCGEx::FPointRef& Next, const double Factor) const
{
	FPCGPoint& MPoint = Point.MutablePoint();
	const FVector Current = MPoint.Transform.GetLocation();

	const FVector PrevPos = Previous.Point->Transform.GetLocation();
	const FVector NextPos = Next.Point->Transform.GetLocation();
	const FVector DirToPrev = (PrevPos - Current);
	const FVector DirToNext = (Current - NextPos);
	const double Weight = PCGExMath::FApex(PrevPos, NextPos, Current).Alpha;

	MPoint.Transform.SetRotation(
		PCGExMath::MakeDirection(
			OrientAxis,
			FMath::Lerp(DirToPrev, DirToNext, bInverseWeight ? 1 - Weight : Weight).GetSafeNormal() * Factor,
			PCGExMath::GetDirection(UpAxis)));
}

void UPCGExOrientWeighted::ApplyOverrides()
{
	Super::ApplyOverrides();
	PCGEX_OVERRIDE_OP_PROPERTY(bInverseWeight, FName(TEXT("Orient/InverseWeight")), EPCGMetadataTypes::Boolean);
}
