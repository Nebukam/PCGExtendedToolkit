// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSubPointsBlendOperation.h"
#include "PCGExSubPointsBlendInheritEnd.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Inherit End")
class PCGEXTENDEDTOOLKIT_API UPCGExSubPointsBlendInheritEnd : public UPCGExSubPointsBlendOperation
{
	GENERATED_BODY()

public:
	virtual void BlendSubPoints(
		const PCGEx::FPointRef& StartPoint,
		const PCGEx::FPointRef& EndPoint,
		TArrayView<FPCGPoint>& SubPoints,
		const PCGExMath::FPathMetrics& Metrics,
		const PCGExDataBlending::FMetadataBlender* InBlender) const override;
};
