// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGPoint.h"
#include "PCGExOperation.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "PCGExTangentsOperation.generated.h"

/**
 * 
 */
UCLASS(Abstract, Blueprintable, EditInlineNew)
class PCGEXTENDEDTOOLKIT_API UPCGExTangentsOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	FName ArriveName = "ArriveTangent";
	FName LeaveName = "LeaveTangent";

	virtual void PrepareForData(PCGExData::FPointIO& InPointIO);
	virtual void ProcessFirstPoint(const PCGEx::FPointRef& MainPoint, const PCGEx::FPointRef& NextPoint, FVector& OutArrive, FVector& OutLeave) const;
	virtual void ProcessLastPoint(const PCGEx::FPointRef& MainPoint, const PCGEx::FPointRef& PreviousPoint, FVector& OutArrive, FVector& OutLeave) const;
	virtual void ProcessPoint(const PCGEx::FPointRef& MainPoint, const PCGEx::FPointRef& PreviousPoint, const PCGEx::FPointRef& NextPoint, FVector& OutArrive, FVector& OutLeave) const;
};
