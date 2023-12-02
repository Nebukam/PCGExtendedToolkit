// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGPoint.h"
#include "PCGExInstruction.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "PCGExTangents.generated.h"

class UPCGExPointIO;
/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew)
class PCGEXTENDEDTOOLKIT_API UPCGExTangents : public UPCGExInstruction
{
	GENERATED_BODY()

public:
	FName ArriveName = "ArriveTangent";
	FName LeaveName = "LeaveTangent";

	FPCGMetadataAttribute<FVector>* ArriveAttribute = nullptr;
	FPCGMetadataAttribute<FVector>* LeaveAttribute = nullptr;

	virtual void PrepareForData(const UPCGExPointIO* InPath);
	virtual void ProcessFirstPoint(const int32 Index, const FPCGPoint& Point, const FPCGPoint& NextPoint) const;
	virtual void ProcessLastPoint(const int32 Index, const FPCGPoint& Point, const FPCGPoint& PreviousPoint) const;
	virtual void ProcessPoint(const int32 Index, const FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const;

protected:
	inline void WriteTangents(const PCGMetadataEntryKey Key, const FVector& Arrive, const FVector& Leave) const;
};
