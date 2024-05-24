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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin =2))
	double BlendRadius = 100;

	/** Defines how fused point properties and attributes are merged together. */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExBlendingSettings BlendingSettings = FPCGExBlendingSettings(EPCGExDataBlendingType::Average);

	virtual void InternalDoSmooth(PCGExData::FPointIO& InPointIO) override;

protected:
	virtual void ApplyOverrides() override;
};
