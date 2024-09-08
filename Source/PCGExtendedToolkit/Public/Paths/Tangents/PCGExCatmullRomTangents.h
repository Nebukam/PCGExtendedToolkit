// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExTangentsOperation.h"
#include "Geometry/PCGExGeo.h"
#include "PCGExCatmullRomTangents.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName="Catmull-Rom")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExCatmullRomTangents : public UPCGExTangentsOperation
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExOperation* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExCatmullRomTangents* TypedOther = Cast<UPCGExCatmullRomTangents>(Other))
		{
		}
	}

	FORCEINLINE virtual void ProcessFirstPoint(const TArray<FPCGPoint>& InPoints, FVector& OutArrive, FVector& OutLeave) const override
	{
		const FVector A = InPoints[0].Transform.GetLocation();
		const FVector C = InPoints[1].Transform.GetLocation();

		const FVector Dir = (C - A);

		OutArrive = (Dir * 0.5f) * ArriveScale;
		OutLeave = (Dir * 0.5f) * LeaveScale;
	}

	FORCEINLINE virtual void ProcessLastPoint(const TArray<FPCGPoint>& InPoints, FVector& OutArrive, FVector& OutLeave) const override
	{
		const FVector A = InPoints.Last().Transform.GetLocation();
		const FVector C = InPoints.Last(1).Transform.GetLocation();

		const FVector Dir = (C - A);

		OutArrive = (Dir * 0.5f) * ArriveScale;
		OutLeave = (Dir * 0.5f) * LeaveScale;
	}

	FORCEINLINE virtual void ProcessPoint(const TArray<FPCGPoint>& InPoints, const int32 Index, const int32 NextIndex, const int32 PrevIndex, FVector& OutArrive, FVector& OutLeave) const override
	{
		const FVector A = InPoints[PrevIndex].Transform.GetLocation();
		const FVector B = InPoints[Index].Transform.GetLocation();
		const FVector C = InPoints[NextIndex].Transform.GetLocation();

		const FVector Dir = (C - A);

		OutArrive = (Dir * 0.5f) * ArriveScale;
		OutLeave = (Dir * 0.5f) * LeaveScale;
	}

protected:
	virtual void ApplyOverrides() override
	{
		Super::ApplyOverrides();
		PCGEX_OVERRIDE_OP_PROPERTY(ArriveScale, FName(TEXT("Tangents/ArriveScale")), EPCGMetadataTypes::Double);
		PCGEX_OVERRIDE_OP_PROPERTY(LeaveScale, FName(TEXT("Tangents/LeaveScale")), EPCGMetadataTypes::Double);
	}
};
