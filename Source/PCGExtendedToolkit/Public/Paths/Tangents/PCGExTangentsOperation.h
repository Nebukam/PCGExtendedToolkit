// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
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
class PCGEXTENDEDTOOLKIT_API UPCGExTangentsOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	FName ArriveName = "ArriveTangent";
	FName LeaveName = "LeaveTangent";

	virtual void PrepareForData(PCGExData::FPointIO* InPointIO);
	virtual void ProcessFirstPoint(const PCGExData::FPointRef& MainPoint, const PCGExData::FPointRef& NextPoint, FVector& OutArrive, FVector& OutLeave) const;
	virtual void ProcessLastPoint(const PCGExData::FPointRef& MainPoint, const PCGExData::FPointRef& PreviousPoint, FVector& OutArrive, FVector& OutLeave) const;
	virtual void ProcessPoint(const PCGExData::FPointRef& MainPoint, const PCGExData::FPointRef& PreviousPoint, const PCGExData::FPointRef& NextPoint, FVector& OutArrive, FVector& OutLeave) const;
};
