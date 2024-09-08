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
	FName ArriveName = "ArriveTangent";
	FName LeaveName = "LeaveTangent";
	double ArriveScale = 1;
	double LeaveScale = 1;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExTangentsOperation* TypedOther = Cast<UPCGExTangentsOperation>(Other))
		{
			ArriveName = TypedOther->ArriveName;
			LeaveName = TypedOther->LeaveName;
			bClosedPath = TypedOther->bClosedPath;
			ArriveScale = TypedOther->ArriveScale;
			LeaveScale = TypedOther->LeaveScale;
		}
	}

	virtual void PrepareForData(PCGExData::FFacade* InDataFacade)
	{
	}

	FORCEINLINE virtual void ProcessFirstPoint(const TArray<FPCGPoint>& InPoints, FVector& OutArrive, FVector& OutLeave) const
	{
	}

	FORCEINLINE virtual void ProcessLastPoint(const TArray<FPCGPoint>& InPoints, FVector& OutArrive, FVector& OutLeave) const
	{
	}

	FORCEINLINE virtual void ProcessPoint(const TArray<FPCGPoint>& InPoints, const int32 Index, const int32 NextIndex, const int32 PrevIndex, FVector& OutArrive, FVector& OutLeave) const
	{
	}
};
