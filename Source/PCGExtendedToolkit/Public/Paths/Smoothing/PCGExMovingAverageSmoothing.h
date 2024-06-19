// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSmoothingOperation.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "PCGExMovingAverageSmoothing.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Moving Average")
class PCGEXTENDEDTOOLKIT_API UPCGExMovingAverageSmoothing : public UPCGExSmoothingOperation
{
	GENERATED_BODY()

public:
	virtual void SmoothSingle(
		PCGExData::FPointIO* Path,
		PCGEx::FPointRef& Target,
		const double Smoothing,
		const double Influence,
		PCGExDataBlending::FMetadataBlender* MetadataBlender,
		const bool bClosedPath) override;
};
