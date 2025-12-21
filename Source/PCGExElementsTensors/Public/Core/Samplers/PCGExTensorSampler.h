// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExTensor.h"
#include "Factories/PCGExInstancedFactory.h"

#include "PCGExTensorSampler.generated.h"

class PCGExTensorOperation;
/**
 * 
 */
UCLASS(DisplayName = "Default", meta=(DisplayName = "Default", ToolTip ="Samples a single location in the tensor field."))
class PCGEXELEMENTSTENSORS_API UPCGExTensorSampler : public UPCGExInstancedFactory
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double Radius = 1;

	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override;
	virtual bool PrepareForData(FPCGExContext* InContext);
	virtual PCGExTensor::FTensorSample RawSample(const TArray<TSharedPtr<PCGExTensorOperation>>& InTensors, int32 InSeedIndex, const FTransform& InProbe) const;
	virtual PCGExTensor::FTensorSample Sample(const TArray<TSharedPtr<PCGExTensorOperation>>& InTensors, int32 InSeedIndex, const FTransform& InProbe, bool& OutSuccess) const;
};
