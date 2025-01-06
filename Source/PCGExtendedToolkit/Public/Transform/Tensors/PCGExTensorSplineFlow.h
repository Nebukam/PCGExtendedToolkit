// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "PCGExTensor.h"
#include "PCGExTensorFactoryProvider.h"
#include "PCGExTensorOperation.h"
#include "PCGExTensorSplineFactoryProvider.h"

#include "PCGExTensorSplineFlow.generated.h"


USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTensorSplineFlowConfig : public FPCGExTensorConfigBase
{
	GENERATED_BODY()

	FPCGExTensorSplineFlowConfig() :
		FPCGExTensorConfigBase()
	{
	}

	/** Sample inputs.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExSplineSamplingIncludeMode SampleInputs = EPCGExSplineSamplingIncludeMode::All;
};

/**
 * 
 */
UCLASS(MinimalAPI)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTensorSplineFlow : public UPCGExTensorPointOperation
{
	GENERATED_BODY()

public:
	FPCGExTensorSplineFlowConfig Config;
	virtual bool Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory) override;

	virtual PCGExTensor::FTensorSample SampleAtPosition(const FVector& InPosition) const override;

protected:
};


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTensorSplineFlowFactory : public UPCGExTensorSplineFactoryData
{
	GENERATED_BODY()

public:
	FPCGExTensorSplineFlowConfig Config;
	virtual UPCGExTensorOperation* CreateOperation(FPCGExContext* InContext) const override;
	virtual bool Prepare(FPCGExContext* InContext) override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Tensors|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExCreateTensorSplineFlowSettings : public UPCGExTensorSplineFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(TensorSplineFlow, "Tensor : Spline Flow", "A tensor that represent a vector/flow field along a spline")

#endif
	//~End UPCGSettings

public:
	/** Tensor properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExTensorSplineFlowConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;
};
