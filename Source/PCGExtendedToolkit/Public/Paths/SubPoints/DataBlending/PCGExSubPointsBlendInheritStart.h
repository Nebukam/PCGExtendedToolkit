// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSubPointsBlendOperation.h"
#include "PCGExSubPointsBlendInheritStart.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Inherit Start")
class PCGEXTENDEDTOOLKIT_API UPCGExSubPointsBlendInheritStart : public UPCGExSubPointsBlendOperation
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
