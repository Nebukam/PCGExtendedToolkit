// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOperation.h"
#include "PCGExTangentsOperation.generated.h"

namespace PCGExData
{
	struct FPointIO;
}

/**
 * 
 */
UCLASS(Abstract)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTangentsOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	bool bClosedPath = false;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExTangentsOperation* TypedOther = Cast<UPCGExTangentsOperation>(Other))
		{
			bClosedPath = TypedOther->bClosedPath;
		}
	}

	virtual void PrepareForData()
	{
	}

	FORCEINLINE virtual void ProcessFirstPoint(
		const TArray<FPCGPoint>& InPoints,
		const FVector& ArriveScale, FVector& OutArrive,
		const FVector& LeaveScale, FVector& OutLeave) const
	{
		const FVector A = InPoints[0].Transform.GetLocation();
		const FVector B = InPoints[1].Transform.GetLocation();
		const FVector Dir = (B - A).GetSafeNormal() * (FVector::Dist(A, B));
		OutArrive = Dir * ArriveScale;
		OutLeave = Dir * LeaveScale;
	}

	FORCEINLINE virtual void ProcessLastPoint(
		const TArray<FPCGPoint>& InPoints,
		const FVector& ArriveScale, FVector& OutArrive,
		const FVector& LeaveScale, FVector& OutLeave) const
	{
		const FVector A = InPoints.Last().Transform.GetLocation();
		const FVector B = InPoints.Last(1).Transform.GetLocation();
		const FVector Dir = (A - B).GetSafeNormal() * (FVector::Dist(A, B));
		OutArrive = Dir * ArriveScale;
		OutLeave = Dir * LeaveScale;
	}

	FORCEINLINE virtual void ProcessPoint(
		const TArray<FPCGPoint>& InPoints,
		const int32 Index, const int32 NextIndex, const int32 PrevIndex,
		const FVector& ArriveScale, FVector& OutArrive,
		const FVector& LeaveScale, FVector& OutLeave) const
	{
	}
};
