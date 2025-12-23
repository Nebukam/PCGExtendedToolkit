// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOrientOperation.h"
#include "Data/PCGExPointElements.h"
#include "Factories/PCGExFactoryData.h"
#include "PCGExOrientWeighted.generated.h"

class FPCGExOrientWeighted : public FPCGExOrientOperation
{
public:
	bool bInverseWeight = false;

	virtual FTransform ComputeOrientation(const PCGExData::FConstPoint& Point, const double DirectionMultiplier) const override
	{
		FTransform OutT = Point.GetTransform();

		const FVector A = Path->GetPos(Point.Index - 1);
		const FVector B = Path->GetPos(Point.Index);
		const FVector C = Path->GetPos(Point.Index + 1);

		const double AB = FVector::DistSquared(A, B);
		const double BC = FVector::DistSquared(B, C);

		const double Weight = (AB + BC) / FMath::Min(AB, BC);

		OutT.SetRotation(PCGExMath::MakeDirection(
			Factory->OrientAxis,
			FMath::Lerp(Path->DirToPrevPoint(Point.Index), Path->DirToNextPoint(Point.Index), bInverseWeight ? 1 - Weight : Weight).GetSafeNormal() * DirectionMultiplier,
			PCGExMath::GetDirection(Factory->UpAxis)));

		return OutT;
	}
};

/**
 * 
 */
UCLASS(MinimalAPI, meta=(DisplayName = "Weighted", PCGExNodeLibraryDoc="paths/orient/orient-weighted"))
class UPCGExOrientWeighted : public UPCGExOrientInstancedFactory
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInverseWeight = false;

	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExOrientWeighted* TypedOther = Cast<UPCGExOrientWeighted>(Other))
		{
			bInverseWeight = TypedOther->bInverseWeight;
		}
	}

	virtual TSharedPtr<FPCGExOrientOperation> CreateOperation() const override
	{
		PCGEX_FACTORY_NEW_OPERATION(OrientWeighted)
		NewOperation->Factory = this;
		NewOperation->bInverseWeight = bInverseWeight;
		return NewOperation;
	}
};
