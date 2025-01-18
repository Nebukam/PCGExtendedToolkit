// Copyright 2025 Timothé Lapetite and contributors
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
		const PCGExPaths::FPathMetrics& Metrics,
		PCGExDataBlending::FMetadataBlender* InBlender, const int32 StartIndex) const override;

	virtual TSharedPtr<PCGExDataBlending::FMetadataBlender> CreateBlender(const TSharedRef<PCGExData::FFacade>& InPrimaryFacade, const TSharedRef<PCGExData::FFacade>& InSecondaryFacade, const PCGExData::ESource SecondarySource, const TSet<FName>* IgnoreAttributeSet) override;
};
