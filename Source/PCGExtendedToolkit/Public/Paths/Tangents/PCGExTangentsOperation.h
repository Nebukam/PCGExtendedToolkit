// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExInstancedFactory.h"
#include "PCGExOperation.h"
#include "PCGExTangentsOperation.generated.h"

namespace PCGExData
{
	class FPointIO;
}

/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExTangentsOperation : public UPCGExInstancedFactory
{
	GENERATED_BODY()

public:
	bool bClosedLoop = false;

	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExTangentsOperation* TypedOther = Cast<UPCGExTangentsOperation>(Other))
		{
			bClosedLoop = TypedOther->bClosedLoop;
		}
	}

	virtual bool PrepareForData(FPCGExContext* InContext)
	{
		return true;
	}

	virtual void ProcessFirstPoint(
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

	virtual void ProcessLastPoint(
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

	virtual void ProcessPoint(
		const TArray<FPCGPoint>& InPoints,
		const int32 Index, const int32 NextIndex, const int32 PrevIndex,
		const FVector& ArriveScale, FVector& OutArrive,
		const FVector& LeaveScale, FVector& OutLeave) const
	{
	}
};
