// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExTangentsOperation.h"

#include "PCGExFromTransformTangents.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName="From Transform")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExFromTransformTangents : public UPCGExTangentsOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Tangents)
	EPCGExAxis Axis = EPCGExAxis::Forward;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExFromTransformTangents* TypedOther = Cast<UPCGExFromTransformTangents>(Other))
		{
			Axis = TypedOther->Axis;
		}
	}

	FORCEINLINE virtual void ProcessFirstPoint(
		const TArray<FPCGPoint>& InPoints,
		const FVector& ArriveScale, FVector& OutArrive,
		const FVector& LeaveScale, FVector& OutLeave) const override
	{
		const FVector Dir = PCGExMath::GetDirection(InPoints[0].Transform.GetRotation(), Axis);
		OutArrive = Dir * ArriveScale;
		OutLeave = Dir * LeaveScale;
	}

	FORCEINLINE virtual void ProcessLastPoint(
		const TArray<FPCGPoint>& InPoints,
		const FVector& ArriveScale, FVector& OutArrive,
		const FVector& LeaveScale, FVector& OutLeave) const override
	{
		const FVector Dir = PCGExMath::GetDirection(InPoints.Last().Transform.GetRotation(), Axis);
		OutArrive = Dir * ArriveScale;
		OutLeave = Dir * LeaveScale;
	}

	FORCEINLINE virtual void ProcessPoint(
		const TArray<FPCGPoint>& InPoints,
		const int32 Index, const int32 NextIndex, const int32 PrevIndex,
		const FVector& ArriveScale, FVector& OutArrive,
		const FVector& LeaveScale, FVector& OutLeave) const override
	{
		const FVector Dir = PCGExMath::GetDirection(InPoints[Index].Transform.GetRotation(), Axis);
		OutArrive = Dir * ArriveScale;
		OutLeave = Dir * LeaveScale;
	}
};
