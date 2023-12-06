// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMath.h"
#include "PCGExTangentsOperation.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "PCGExAutoTangents.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew)
class PCGEXTENDEDTOOLKIT_API UPCGExAutoTangents : public UPCGExTangentsOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	double Scale = 1;

	virtual void ProcessFirstPoint(const int32 Index, const FPCGPoint& Point, const FPCGPoint& NextPoint) const override;
	virtual void ProcessLastPoint(const int32 Index, const FPCGPoint& Point, const FPCGPoint& PreviousPoint) const override;
	virtual void ProcessPoint(const int32 Index, const FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const override;
};
