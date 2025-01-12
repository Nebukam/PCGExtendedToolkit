// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "PCGExTensor.h"
#include "PCGExTensorFactoryProvider.h"
#include "PCGExTensorOperation.h"

#include "PCGExTensorFlow.generated.h"


USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTensorFlowConfig : public FPCGExTensorConfigBase
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

	/** Direction axis, read from the input points' transform.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName="Direction", EditCondition="DirectionInput == EPCGExInputValueType::Constant", EditConditionHides))
	EPCGExAxis DirectionConstant = EPCGExAxis::Forward;

	/** Fetch the direction from a local attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Direction", EditCondition="DirectionInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector DirectionAttribute;

	/** Whether the direction is absolute or should be transformed by the owner' transform .*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="DirectionInput != EPCGExInputValueType::Constant", EditConditionHides))
	EPCGExTransformMode DirectionTransform = EPCGExTransformMode::Relative;
};

/**
 * 
 */
UCLASS(MinimalAPI)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTensorFlow : public UPCGExTensorPointOperation
{
	GENERATED_BODY()

public:
	FPCGExTensorFlowConfig Config;
	virtual bool Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory) override;

	virtual PCGExTensor::FTensorSample Sample(const FTransform& InProbe) const override;
};


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTensorFlowFactory : public UPCGExTensorPointFactoryData
{
	GENERATED_BODY()

public:
	FPCGExTensorFlowConfig Config;
	virtual UPCGExTensorOperation* CreateOperation(FPCGExContext* InContext) const override;

protected:
	TSharedPtr<PCGExData::TBuffer<FVector>> DirectionBuffer;

	virtual bool InitInternalData(FPCGExContext* InContext) override;
	virtual bool InitInternalFacade(FPCGExContext* InContext) override;
	virtual void PrepareSinglePoint(int32 Index, FPCGPoint& InPoint) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Tensors|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExCreateTensorFlowSettings : public UPCGExTensorPointFactoryProviderSettings
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
