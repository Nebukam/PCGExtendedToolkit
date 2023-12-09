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
UCLASS(Blueprintable, EditInlineNew, HideDropdown)
class PCGEXTENDEDTOOLKIT_API UPCGExTangentsOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	FName ArriveName = "ArriveTangent";
	FName LeaveName = "LeaveTangent";

	FPCGMetadataAttribute<FVector>* ArriveAttribute = nullptr;
	FPCGMetadataAttribute<FVector>* LeaveAttribute = nullptr;

	virtual void PrepareForData(PCGExData::FPointIO& InPath);
	virtual void ProcessFirstPoint(const int32 Index, const FPCGPoint& Point, const FPCGPoint& NextPoint) const;
	virtual void ProcessLastPoint(const int32 Index, const FPCGPoint& Point, const FPCGPoint& PreviousPoint) const;
	virtual void ProcessPoint(const int32 Index, const FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const;

protected:
	inline void WriteTangents(const PCGMetadataEntryKey Key, const FVector& Arrive, const FVector& Leave) const;
};
