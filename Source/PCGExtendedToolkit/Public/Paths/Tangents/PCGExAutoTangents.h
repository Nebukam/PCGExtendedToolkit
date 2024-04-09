// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExTangentsOperation.h"
#include "PCGExAutoTangents.generated.h"

/**
 * 
 */
UCLASS(DisplayName="Auto")
class PCGEXTENDEDTOOLKIT_API UPCGExAutoTangents : public UPCGExTangentsOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double Scale = 1;

	virtual void ProcessFirstPoint(const PCGEx::FPointRef& MainPoint, const PCGEx::FPointRef& NextPoint, FVector& OutArrive, FVector& OutLeave) const override;
	virtual void ProcessLastPoint(const PCGEx::FPointRef& MainPoint, const PCGEx::FPointRef& PreviousPoint, FVector& OutArrive, FVector& OutLeave) const override;
	virtual void ProcessPoint(const PCGEx::FPointRef& MainPoint, const PCGEx::FPointRef& PreviousPoint, const PCGEx::FPointRef& NextPoint, FVector& OutArrive, FVector& OutLeave) const override;

protected:
	virtual void ApplyOverrides() override;
};
