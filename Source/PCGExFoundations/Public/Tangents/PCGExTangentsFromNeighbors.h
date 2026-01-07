// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExTangentsInstancedFactory.h"
#include "Factories/PCGExFactoryData.h"

#include "PCGExTangentsFromNeighbors.generated.h"

class FPCGExTangentsFromNeighbors : public FPCGExTangentsOperation
{
public:
	virtual void ProcessPoint(const UPCGBasePointData* InPointData, const int32 Index, const int32 NextIndex, const int32 PrevIndex, const FVector& ArriveScale, FVector& OutArrive, const FVector& LeaveScale, FVector& OutLeave) const override
	{
		const TConstPCGValueRange<FTransform> InTransforms = InPointData->GetConstTransformValueRange();

		const FVector PrevDir = (InTransforms[PrevIndex].GetLocation() - InTransforms[Index].GetLocation()) * -1;
		const FVector NextDir = InTransforms[NextIndex].GetLocation() - InTransforms[Index].GetLocation();
		const FVector Dir = FMath::Lerp(PrevDir, NextDir, 0.5);
		OutArrive = Dir * ArriveScale;
		OutLeave = Dir * LeaveScale;
	}
};

/**
 * 
 */
UCLASS(MinimalAPI, meta=(DisplayName = "From Neighbors", PCGExNodeLibraryDoc="paths/write-tangents/tangents-neighbors"))
class UPCGExFromNeighborsTangents : public UPCGExTangentsInstancedFactory
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<FPCGExTangentsOperation> CreateOperation() const override
	{
		PCGEX_FACTORY_NEW_OPERATION(TangentsFromNeighbors)
		return NewOperation;
	}
};
