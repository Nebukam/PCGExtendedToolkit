// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOperation.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "PCGExSmoothingOperation.generated.h"

/**
 * 
 */
UCLASS(Abstract)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSmoothingOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	virtual void SmoothSingle(
		const TSharedRef<PCGExData::FPointIO>& Path,
		PCGExData::FPointRef& Target,
		const double Smoothing,
		const double Influence,
		PCGExDataBlending::FMetadataBlender* MetadataBlender,
		const bool bClosedLoop)
	{
	}
};
