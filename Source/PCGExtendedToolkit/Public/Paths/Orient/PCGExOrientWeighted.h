// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOrientOperation.h"
#include "PCGExOrientWeighted.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Weighted")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExOrientWeighted : public UPCGExOrientOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInverseWeight = false;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExOrientWeighted* TypedOther = Cast<UPCGExOrientWeighted>(Other))
		{
			bInverseWeight = TypedOther->bInverseWeight;
		}
	}

	virtual FTransform ComputeOrientation(
		const PCGExData::FPointRef& Point,
		const double DirectionMultiplier) const override
	{
		FTransform OutT = Point.MutablePoint().Transform;

		const FVector A = Path->GetPos(Point.Index - 1);
		const FVector B = Path->GetPos(Point.Index);
		const FVector C = Path->GetPos(Point.Index + 1);

		const double AB = FVector::DistSquared(A, B);
		const double BC = FVector::DistSquared(B, C);

		const double Weight = (AB + BC) / FMath::Min(AB, BC);

		OutT.SetRotation(
			PCGExMath::MakeDirection(
				OrientAxis,
				FMath::Lerp(Path->DirToPrevPoint(Point.Index), Path->DirToNextPoint(Point.Index), bInverseWeight ? 1 - Weight : Weight).GetSafeNormal() * DirectionMultiplier,
				PCGExMath::GetDirection(UpAxis)));

		return OutT;
	}
};
