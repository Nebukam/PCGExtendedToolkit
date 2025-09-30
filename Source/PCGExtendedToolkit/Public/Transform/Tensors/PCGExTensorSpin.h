﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "PCGExTensor.h"
#include "PCGExTensorFactoryProvider.h"
#include "PCGExTensorOperation.h"
#include "Transform/PCGExTransform.h"


#include "PCGExTensorSpin.generated.h"


USTRUCT(BlueprintType)
struct FPCGExTensorSpinConfig : public FPCGExTensorConfigBase
{
	GENERATED_BODY()

	FPCGExTensorSpinConfig() :
		FPCGExTensorConfigBase()
	{
	}

	/** Direction type.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExInputValueType AxisInput = EPCGExInputValueType::Constant;

	/** Fetch the direction from a local attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Axis (Attr)", EditCondition="AxisInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector AxisAttribute;

	/** Direction axis, read from the input points' transform.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName="Axis", EditCondition="AxisInput == EPCGExInputValueType::Constant", EditConditionHides))
	EPCGExAxis AxisConstant = EPCGExAxis::Up;

	/** Whether the direction is absolute or should be transformed by the owner' transform .*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="AxisInput != EPCGExInputValueType::Constant", EditConditionHides))
	EPCGExTransformMode AxisTransform = EPCGExTransformMode::Relative;
};

namespace PCGExTensor
{
	class FSpinEffectorsArray : public FEffectorsArray
	{
		FPCGExTensorSpinConfig Config;
		TSharedPtr<PCGExData::TBuffer<FVector>> AxisBuffer;

	public:
		virtual bool Init(FPCGExContext* InContext, const UPCGExTensorPointFactoryData* InFactory) override;

	protected:
		virtual void PrepareSinglePoint(const int32 Index) override;
	};
}

/**
 * 
 */
class FPCGExTensorSpin : public PCGExTensorPointOperation
{
public:
	FPCGExTensorSpinConfig Config;

	virtual bool Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory) override;

	virtual PCGExTensor::FTensorSample Sample(int32 InSeedIndex, const FTransform& InProbe) const override;
};


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExTensorSpinFactory : public UPCGExTensorPointFactoryData
{
	GENERATED_BODY()

public:
	FPCGExTensorSpinConfig Config;
	virtual TSharedPtr<PCGExTensorOperation> CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Tensors|Params", meta=(PCGExNodeLibraryDoc="tensors/effectors/tensor-spin"))
class UPCGExCreateTensorSpinSettings : public UPCGExTensorPointFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(TensorSpin, "Tensor : Spin", "A tensor that represent a spin around a given axis")

#endif
	//~End UPCGSettings

	/** Tensor properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExTensorSpinConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;
};
