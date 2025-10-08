﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "PCGExTensor.h"
#include "PCGExTensorFactoryProvider.h"
#include "PCGExTensorOperation.h"
#include "Transform/PCGExTransform.h"

#include "PCGExTensorFlow.generated.h"


USTRUCT(BlueprintType)
struct FPCGExTensorFlowConfig : public FPCGExTensorConfigBase
{
	GENERATED_BODY()

	FPCGExTensorFlowConfig() :
		FPCGExTensorConfigBase()
	{
		DirectionAttribute.Update(TEXT("$Rotation.Forward"));
	}

	/** Direction type.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExInputValueType DirectionInput = EPCGExInputValueType::Constant;

	/** Fetch the direction from a local attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Direction (Attr)", EditCondition="DirectionInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector DirectionAttribute;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Invert", EditCondition="DirectionInput != EPCGExInputValueType::Constant", EditConditionHides))
	bool bInvertDirection = false;

	/** Direction axis, read from the input points' transform.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName="Direction", EditCondition="DirectionInput == EPCGExInputValueType::Constant", EditConditionHides))
	EPCGExAxis DirectionConstant = EPCGExAxis::Forward;

	/** Whether the direction is absolute or should be transformed by the owner' transform .*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="DirectionInput != EPCGExInputValueType::Constant", EditConditionHides))
	EPCGExTransformMode DirectionTransform = EPCGExTransformMode::Relative;
};

/**
 * 
 */
class FPCGExTensorFlow : public PCGExTensorPointOperation
{
public:
	FPCGExTensorFlowConfig Config;
	virtual bool Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory) override;

	virtual PCGExTensor::FTensorSample Sample(int32 InSeedIndex, const FTransform& InProbe) const override;
};

namespace PCGExTensor
{
	class FFlowEffectorsArray : public FEffectorsArray
	{
		FPCGExTensorFlowConfig Config;
		TSharedPtr<PCGExData::TBuffer<FVector>> DirectionBuffer;
		double DirectionMultiplier = 1;

	public:
		virtual bool Init(FPCGExContext* InContext, const UPCGExTensorPointFactoryData* InFactory) override;

	protected:
		virtual void PrepareSinglePoint(const int32 Index) override;
	};
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExTensorFlowFactory : public UPCGExTensorPointFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExTensorFlowConfig Config;

	virtual TSharedPtr<PCGExTensorOperation> CreateOperation(FPCGExContext* InContext) const override;

protected:
	virtual TSharedPtr<PCGExTensor::FEffectorsArray> GetEffectorsArray() const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Tensors|Params", meta=(PCGExNodeLibraryDoc="tensors/effectors/tensor-inertia-constant"))
class UPCGExCreateTensorFlowSettings : public UPCGExTensorPointFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(TensorFlow, "Tensor : Flow", "A tensor that represent a vector/flow field")

#endif
	//~End UPCGSettings

	/** Tensor properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExTensorFlowConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;
};
