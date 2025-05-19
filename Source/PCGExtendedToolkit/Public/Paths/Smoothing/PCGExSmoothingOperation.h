// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExInstancedFactory.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "PCGExSmoothingOperation.generated.h"

/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExSmoothingOperation : public UPCGExInstancedFactory
{
	GENERATED_BODY()

public:
	virtual void SmoothSingle(
		const TSharedRef<PCGExData::FPointIO>& Path,
		PCGExData::FConstPoint& Target,
		const double Smoothing,
		const double Influence,
		PCGExDataBlending::FMetadataBlender* MetadataBlender,
		const bool bClosedLoop)
	{
	}
};
