// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExInstancedFactory.h"
#include "PCGExOperation.h"
#include "PCGExTangentsInstancedFactory.generated.h"

namespace PCGExData
{
	class FPointIO;
}

class FPCGExTangentsOperation : public FPCGExOperation
{
public:
	bool bClosedLoop = false;

	virtual bool PrepareForData(FPCGExContext* InContext)
	{
		return true;
	}

	virtual void ProcessFirstPoint(
		const UPCGBasePointData* InPointData,
		const FVector& ArriveScale, FVector& OutArrive,
		const FVector& LeaveScale, FVector& OutLeave) const
	{
		const TConstPCGValueRange<FTransform> InTransforms = InPointData->GetConstTransformValueRange();

		const FVector A = InTransforms[0].GetLocation();
		const FVector B = InTransforms[1].GetLocation();
		const FVector Dir = (B - A).GetSafeNormal() * (FVector::Dist(A, B));
		OutArrive = Dir * ArriveScale;
		OutLeave = Dir * LeaveScale;
	}

	virtual void ProcessLastPoint(
		const UPCGBasePointData* InPointData,
		const FVector& ArriveScale, FVector& OutArrive,
		const FVector& LeaveScale, FVector& OutLeave) const
	{
		const TConstPCGValueRange<FTransform> InTransforms = InPointData->GetConstTransformValueRange();
		const int32 LastIndex = InPointData->GetNumPoints() - 1;

		const FVector A = InTransforms[LastIndex].GetLocation();
		const FVector B = InTransforms[LastIndex - 1].GetLocation();
		const FVector Dir = (A - B).GetSafeNormal() * (FVector::Dist(A, B));
		OutArrive = Dir * ArriveScale;
		OutLeave = Dir * LeaveScale;
	}

	virtual void ProcessPoint(
		const UPCGBasePointData* InPointData,
		const int32 Index, const int32 NextIndex, const int32 PrevIndex,
		const FVector& ArriveScale, FVector& OutArrive,
		const FVector& LeaveScale, FVector& OutLeave) const
	{
	}
};

/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExTangentsInstancedFactory : public UPCGExInstancedFactory
{
	GENERATED_BODY()

public:
	bool bClosedLoop = false;

	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExTangentsInstancedFactory* TypedOther = Cast<UPCGExTangentsInstancedFactory>(Other))
		{
			bClosedLoop = TypedOther->bClosedLoop;
		}
	}

	virtual TSharedPtr<FPCGExTangentsOperation> CreateOperation() const
	PCGEX_NOT_IMPLEMENTED_RET(CreateOperation(), nullptr);
};
