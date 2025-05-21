// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExInstancedFactory.h"
#include "Data/Blending/PCGExMetadataBlender.h"





#include "PCGExSmoothingInstancedFactory.generated.h"

class FPCGExSmoothingOperation : public FPCGExOperation
{
public:
	virtual void SmoothSingle(
		const TSharedRef<PCGExData::FPointIO>& Path, PCGExData::FConstPoint& Target,
		const double Smoothing, const double Influence,
		const TSharedRef<PCGExDataBlending::FMetadataBlender>& Blender,
		const bool bClosedLoop)
	{
	}
};

/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExSmoothingInstancedFactory : public UPCGExInstancedFactory
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<FPCGExSmoothingOperation> CreateOperation() const
	PCGEX_NOT_IMPLEMENTED_RET(CreateOperation(), nullptr);
};
