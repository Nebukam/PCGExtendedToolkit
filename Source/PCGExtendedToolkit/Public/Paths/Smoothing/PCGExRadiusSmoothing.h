// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSmoothingOperation.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "PCGExRadiusSmoothing.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Radius")
class PCGEXTENDEDTOOLKIT_API UPCGExRadiusSmoothing : public UPCGExSmoothingOperation
{
	GENERATED_BODY()

public:
	virtual void DoSmooth(
		PCGExData::FPointIO& InPointIO,
		const TArray<double>* Smoothing,
		const TArray<double>* Influence,
		const bool bClosedPath,
		const FPCGExBlendingSettings* BlendingSettings) override;
};
