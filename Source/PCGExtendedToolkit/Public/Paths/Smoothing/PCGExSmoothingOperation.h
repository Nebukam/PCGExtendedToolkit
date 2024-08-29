// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
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
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSmoothingOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	virtual void SmoothSingle(
		PCGExData::FPointIO* Path,
		PCGExData::FPointRef& Target,
		const double Smoothing,
		const double Influence,
		PCGExDataBlending::FMetadataBlender* MetadataBlender,
		const bool bClosedPath);
};
