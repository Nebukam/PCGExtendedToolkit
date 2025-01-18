// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExTangentsOperation.h"
#include "Geometry/PCGExGeo.h"
#include "PCGExAutoTangents.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName="Auto")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExAutoTangents : public UPCGExTangentsOperation
{
	GENERATED_BODY()

public:
	FORCEINLINE virtual void ProcessPoint(
		const TArray<FPCGPoint>& InPoints,
		const int32 Index, const int32 NextIndex, const int32 PrevIndex,
		const FVector& ArriveScale, FVector& OutArrive,
		const FVector& LeaveScale, FVector& OutLeave) const override
	{
		const PCGExGeo::FApex Apex = PCGExGeo::FApex(
			InPoints[PrevIndex].Transform.GetLocation(),
			InPoints[NextIndex].Transform.GetLocation(),
			InPoints[Index].Transform.GetLocation());

		OutArrive = Apex.TowardStart * ArriveScale;
		OutLeave = Apex.TowardEnd * -1 * LeaveScale;
	}
};
