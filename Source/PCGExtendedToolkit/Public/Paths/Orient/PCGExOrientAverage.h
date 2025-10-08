// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOrientOperation.h"

// @SPLASH_DAMAGE_CHANGE [IMPROVEMENT] #SDTechArt - BEGIN: Fixing Non Unity Warnings ("C3861: '####': identifier not found") by adding missing Unreal Math and PCGEx headers
#include "Data/PCGExPointElements.h"
#include "Math/MathFwd.h"
// @SPLASH_DAMAGE_CHANGE [IMPROVEMENT] #SDTechArt - END: Fixing Non Unity Warnings ("C3861: '####': identifier not found") by adding missing Unreal Math and PCGEx headers

#include "PCGExOrientAverage.generated.h"

class FPCGExOrientAverage : public FPCGExOrientOperation
{
public:
	virtual FTransform ComputeOrientation(
		const PCGExData::FConstPoint& Point,
		const double DirectionMultiplier) const override
	{
		const FVector A = Path->DirToNextPoint(Point.Index);
		const FVector B = Path->DirToPrevPoint(Point.Index) * -1;
		FTransform OutT = Point.GetTransform();
		OutT.SetRotation(
			PCGExMath::MakeDirection(
				Factory->OrientAxis, FMath::Lerp(A, B, 0.5).GetSafeNormal() * DirectionMultiplier,
				PCGExMath::GetDirection(Factory->UpAxis)));
		return OutT;
	}
};

/**
 * 
 */
UCLASS(MinimalAPI, meta=(DisplayName = "Average", PCGExNodeLibraryDoc="paths/orient/orient-average"))
class UPCGExOrientAverage : public UPCGExOrientInstancedFactory
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<FPCGExOrientOperation> CreateOperation() const override
	{
		PCGEX_FACTORY_NEW_OPERATION(OrientAverage)
		NewOperation->Factory = this;
		return NewOperation;
	}
};
