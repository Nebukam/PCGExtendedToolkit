// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExAttributeHelpers.h"
#include "PCGExOperation.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "PCGExSmoothingOperation.generated.h"

namespace PCGExDataBlending
{
	struct FPropertiesBlender;
	class FMetadataBlender;
}

/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExSmoothingOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	virtual void DoSmooth(
		PCGExData::FPointIO& InPointIO,
		const TArray<double>* Smoothing,
		const TArray<double>* Influence,
		const bool bClosedPath, const FPCGExBlendingSettings* BlendingSettings);
};
