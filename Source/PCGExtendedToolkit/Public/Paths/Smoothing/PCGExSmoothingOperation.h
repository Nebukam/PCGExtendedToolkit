﻿// Copyright Timothé Lapetite 2023
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
	virtual void DoSmooth(PCGExData::FPointIO& InPointIO);

	bool bPinStart = true;
	bool bPinEnd = true;
	double FixedInfluence = 1.0;
	bool bUseLocalInfluence = false;
	FPCGExInputDescriptorWithSingleField InfluenceDescriptor;

protected:
	FPCGExBlendingSettings InfluenceSettings = FPCGExBlendingSettings(EPCGExDataBlendingType::Weight);
	virtual void InternalDoSmooth(
		PCGExData::FPointIO& InPointIO);
};
