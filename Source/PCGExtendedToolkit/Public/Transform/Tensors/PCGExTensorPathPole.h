// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "PCGExTensor.h"
#include "PCGExTensorFactoryProvider.h"
#include "PCGExTensorOperation.h"
#include "PCGExTensorSplineFactoryProvider.h"
#include "PCGExTensorSplineFlow.h"

#include "PCGExTensorPathPole.generated.h"


USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTensorPathPoleConfig : public FPCGExTensorConfigBase
{
	GENERATED_BODY()

	FPCGExTensorPathPoleConfig() :
		FPCGExTensorConfigBase(false)
	{
	}

	/** Closed loop handling.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExPathClosedLoopDetails ClosedLoop;

	/** Which point type to use. Shared amongst all points; if you want tight control, create a fully-fledged spline instead. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExSplinePointTypeRedux PointType = EPCGExSplinePointTypeRedux::Linear;

	/** Sample inputs.*/
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExSplineSamplingIncludeMode SampleInputs = EPCGExSplineSamplingIncludeMode::All;

	/**  Base radius of the spline. Will be scaled by control points' scale length */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayAfter="TensorWeight"))
	double Radius = 100;
};

/**
 * 
 */
UCLASS(MinimalAPI)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTensorPathPole : public UPCGExTensorOperation
{
	GENERATED_BODY()

public:
	FPCGExTensorPathPoleConfig Config;
	const TArray<TSharedPtr<const FPCGSplineStruct>>* Splines = nullptr;

	virtual PCGExTensor::FTensorSample Sample(const FTransform& InProbe) const override;
};


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTensorPathPoleFactory : public UPCGExTensorSplineFlowFactory
{
	GENERATED_BODY()

public:
	FPCGExTensorPathPoleConfig Config;
	virtual UPCGExTensorOperation* CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Tensors|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExCreateTensorPathPoleSettings : public UPCGExTensorSplineFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(TensorPathPole, "Tensor : Path Pole", "A tensor that represent a vector/flow field along a path")

#endif
	//~End UPCGSettings

	virtual bool GetBuildFromPoints() const override { return true; }

	/** Tensor properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExTensorPathPoleConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;
};
