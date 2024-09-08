// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSubPointsBlendOperation.h"
#include "PCGExSubPointsBlendInheritEnd.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Inherit Last")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSubPointsBlendInheritEnd : public UPCGExSubPointsBlendOperation
{
	GENERATED_BODY()

public:
	virtual void BlendSubPoints(
		const PCGExData::FPointRef& From,
		const PCGExData::FPointRef& To,
		const TArrayView<FPCGPoint>& SubPoints,
		const PCGExPaths::FPathMetrics& Metrics,
		PCGExDataBlending::FMetadataBlender* InBlender,
		const int32 StartIndex) const override;
};
