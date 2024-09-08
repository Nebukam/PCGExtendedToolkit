// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExTangentsOperation.h"
#include "Data/PCGExAttributeHelpers.h"

#include "PCGExFromNeighborsTangents.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName="From Neighbors")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExFromNeighborsTangents : public UPCGExTangentsOperation
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExOperation* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExFromNeighborsTangents* TypedOther = Cast<UPCGExFromNeighborsTangents>(Other))
		{
		}
	}

	FORCEINLINE virtual void ProcessFirstPoint(const TArray<FPCGPoint>& InPoints, FVector& OutArrive, FVector& OutLeave) const override
	{
		const FVector Dir = (InPoints[1].Transform.GetLocation() - InPoints[0].Transform.GetLocation()).GetSafeNormal();
		OutArrive = Dir;
		OutLeave = Dir;
	}

	FORCEINLINE virtual void ProcessLastPoint(const TArray<FPCGPoint>& InPoints, FVector& OutArrive, FVector& OutLeave) const override
	{
		const FVector Dir = (InPoints[InPoints.Num() - 2].Transform.GetLocation() - InPoints[InPoints.Num() - 1].Transform.GetLocation()).GetSafeNormal() * -1;
		OutArrive = Dir;
		OutLeave = Dir;
	}

	FORCEINLINE virtual void ProcessPoint(const TArray<FPCGPoint>& InPoints, const int32 Index, const int32 NextIndex, const int32 PrevIndex, FVector& OutArrive, FVector& OutLeave) const override
	{
		const FVector PrevDir = (InPoints[PrevIndex].Transform.GetLocation() - InPoints[Index].Transform.GetLocation()) * -1;
		const FVector NextDir = InPoints[NextIndex].Transform.GetLocation() - InPoints[Index].Transform.GetLocation();
		const FVector Dir = FMath::Lerp(PrevDir, NextDir, 0.5);
		OutArrive = Dir * ArriveScale;
		OutLeave = Dir * LeaveScale;
	}
};
