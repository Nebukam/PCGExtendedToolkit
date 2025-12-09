// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExTangentsInstancedFactory.h"
#include "PCGExTangentsZero.generated.h"

class FPCGExTangentsZero : public FPCGExTangentsOperation
{
public:
	FORCEINLINE virtual void ProcessFirstPoint(const UPCGBasePointData* InPointData, const FVector& ArriveScale, FVector& OutArrive, const FVector& LeaveScale, FVector& OutLeave) const override
	{
		OutArrive = FVector::ZeroVector;
		OutLeave = FVector::ZeroVector;
	}

	FORCEINLINE virtual void ProcessLastPoint(const UPCGBasePointData* InPointData, const FVector& ArriveScale, FVector& OutArrive, const FVector& LeaveScale, FVector& OutLeave) const override
	{
		OutArrive = FVector::ZeroVector;
		OutLeave = FVector::ZeroVector;
	}

	FORCEINLINE virtual void ProcessPoint(const UPCGBasePointData* InPointData, const int32 Index, const int32 NextIndex, const int32 PrevIndex, const FVector& ArriveScale, FVector& OutArrive, const FVector& LeaveScale, FVector& OutLeave) const override
	{
		OutArrive = FVector::ZeroVector;
		OutLeave = FVector::ZeroVector;
	}
};

/**
 * 
 */
UCLASS(MinimalAPI, meta=(DisplayName = "Zero", PCGExNodeLibraryDoc="paths/write-tangents/tangents-zero"))
class UPCGExZeroTangents : public UPCGExTangentsInstancedFactory
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<FPCGExTangentsOperation> CreateOperation() const override
	{
		PCGEX_FACTORY_NEW_OPERATION(TangentsZero)
		return NewOperation;
	}
};
