// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSubPointsBlendOperation.h"
#include "PCGExSubPointsBlendNone.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "No Blending")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSubPointsBlendNone : public UPCGExSubPointsBlendOperation
{
	GENERATED_BODY()

public:
	virtual void BlendSubPoints(
		const PCGExData::FPointRef& From,
		const PCGExData::FPointRef& To,
		const TArrayView<FPCGPoint>& SubPoints,
		const PCGExMath::FPathMetricsSquared& Metrics,
		PCGExDataBlending::FMetadataBlender* InBlender, const int32 StartIndex) const override;

	virtual PCGExDataBlending::FMetadataBlender* CreateBlender(PCGExData::FFacade* InPrimaryFacade, PCGExData::FFacade* InSecondaryFacade, const PCGExData::ESource SecondarySource) override;
};
