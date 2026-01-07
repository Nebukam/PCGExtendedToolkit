// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExTangentsInstancedFactory.h"
#include "Math/Geo/PCGExGeo.h"

#include "PCGExTangentsAuto.generated.h"

class FPCGExTangentsAuto : public FPCGExTangentsOperation
{
public:
	virtual void ProcessPoint(const UPCGBasePointData* InPointData, const int32 Index, const int32 NextIndex, const int32 PrevIndex, const FVector& ArriveScale, FVector& OutArrive, const FVector& LeaveScale, FVector& OutLeave) const override
	{
		TConstPCGValueRange<FTransform> InTransforms = InPointData->GetConstTransformValueRange();

		const PCGExMath::Geo::FApex Apex = PCGExMath::Geo::FApex(InTransforms[PrevIndex].GetLocation(), InTransforms[NextIndex].GetLocation(), InTransforms[Index].GetLocation());

		OutArrive = Apex.TowardStart * ArriveScale;
		OutLeave = Apex.TowardEnd * -1 * LeaveScale;
	}
};

/**
 * 
 */
UCLASS(MinimalAPI, meta=(DisplayName = "Auto", PCGExNodeLibraryDoc="paths/write-tangents/tangents-auto"))
class UPCGExAutoTangents : public UPCGExTangentsInstancedFactory
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<FPCGExTangentsOperation> CreateOperation() const override
	{
		PCGEX_FACTORY_NEW_OPERATION(TangentsAuto)
		return NewOperation;
	}
};
