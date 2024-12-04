// Copyright 2024 Timothé Lapetite and contributors
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
	FORCEINLINE virtual void ProcessPoint(
		const TArray<FPCGPoint>& InPoints,
		const int32 Index, const int32 NextIndex, const int32 PrevIndex,
		const FVector& ArriveScale, FVector& OutArrive,
		const FVector& LeaveScale, FVector& OutLeave) const override
	{
		const FVector A = InPoints[PrevIndex].Transform.GetLocation();
		const FVector B = InPoints[Index].Transform.GetLocation();
		const FVector C = InPoints[NextIndex].Transform.GetLocation();

		const FVector Dir = (C - A);

		OutArrive = (Dir * 0.5f) * ArriveScale;
		OutLeave = (Dir * 0.5f) * LeaveScale;
	}
};
