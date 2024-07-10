// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOrientOperation.h"
#include "PCGExOrientWeighted.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Weighted")
class PCGEXTENDEDTOOLKIT_API UPCGExOrientWeighted : public UPCGExOrientOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInverseWeight = false;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual FTransform ComputeOrientation(const PCGEx::FPointRef& Point, const PCGEx::FPointRef& Previous, const PCGEx::FPointRef& Next, const double DirectionMultiplier) const override;

protected:
	virtual void ApplyOverrides() override;
};
