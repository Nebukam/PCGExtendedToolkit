// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once
#include "PCGExDetails.h"
#include "Data/PCGExData.h"
#include "PCGExTensor.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTensorConfigBase
{
	GENERATED_BODY()

	FPCGExTensorConfigBase()
	{
	}

	virtual ~FPCGExTensorConfigBase()
	{
	}

	/** Resolution input type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta = (PCG_NotOverridable))
	EPCGExInputValueType WeightInput = EPCGExInputValueType::Constant;

	/** Resolution Constant. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable, DisplayName="Weight", EditCondition="WeightInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=0))
	double ResolutionConstant = 1;

	/** Resolution Attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable, DisplayName="Weight", EditCondition="WeightInput == EPCGExInputValueType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector WeightAttribute;

	/** Uniform weight factor for this tensor. Multiplier applied to individual output values. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable, EditCondition="ResolutionInput == EPCGExInputValueType::Constant"))
	double UniformWeightFactor = 1;

	virtual void Init()
	{
		
	}
};

namespace PCGExTensor
{
	const FName OutputTensorLabel = TEXT("Tensor");
	const FName SourceTensorsLabel = TEXT("Tensors");

	struct FSampleDirection
	{
		FVector4 Direction;
	};
}