// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOrientOperation.h"
#include "PCGExOrientAverage.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Average")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExOrientAverage : public UPCGExOrientOperation
{
	GENERATED_BODY()

public:
	virtual FTransform ComputeOrientation(const PCGExData::FPointRef& Point, const PCGExData::FPointRef& Previous, const PCGExData::FPointRef& Next, const double DirectionMultiplier) const override;
};
