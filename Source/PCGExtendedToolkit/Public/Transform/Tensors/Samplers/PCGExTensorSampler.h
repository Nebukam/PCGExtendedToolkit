// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExOperation.h"
#include "Transform/Tensors/PCGExTensor.h"
#include "Transform/Tensors/PCGExTensorOperation.h"

#include "PCGExTensorSampler.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Default", meta=(DisplayName = "Default", ToolTip ="Samples a single location in the tensor field."))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTensorSampler : public UPCGExOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double Radius = 1;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;
	virtual bool PrepareForData(FPCGExContext* InContext);
	virtual PCGExTensor::FTensorSample RawSample(const TArray<UPCGExTensorOperation*>& InTensors, const FTransform& InProbe) const;
	virtual PCGExTensor::FTensorSample Sample(const TArray<UPCGExTensorOperation*>& InTensors, const FTransform& InProbe, bool& OutSuccess) const;
};
