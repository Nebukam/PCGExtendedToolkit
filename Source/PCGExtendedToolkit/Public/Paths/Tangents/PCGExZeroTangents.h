// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExTangentsOperation.h"
#include "PCGExZeroTangents.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName="Zero")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExZeroTangents : public UPCGExTangentsOperation
{
	GENERATED_BODY()

public:
	FORCEINLINE virtual void ProcessFirstPoint(const TArray<FPCGPoint>& InPoints, FVector& OutArrive, FVector& OutLeave) const override
	{
		OutArrive = FVector::ZeroVector;
		OutLeave = FVector::ZeroVector;
	}

	FORCEINLINE virtual void ProcessLastPoint(const TArray<FPCGPoint>& InPoints, FVector& OutArrive, FVector& OutLeave) const override
	{
		OutArrive = FVector::ZeroVector;
		OutLeave = FVector::ZeroVector;
	}

	FORCEINLINE virtual void ProcessPoint(const TArray<FPCGPoint>& InPoints, const int32 Index, const int32 NextIndex, const int32 PrevIndex, FVector& OutArrive, FVector& OutLeave) const override
	{
		OutArrive = FVector::ZeroVector;
		OutLeave = FVector::ZeroVector;
	}
};
