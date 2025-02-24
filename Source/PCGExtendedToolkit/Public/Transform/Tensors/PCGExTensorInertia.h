// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "PCGExTensor.h"
#include "PCGExTensorFactoryProvider.h"
#include "PCGExTensorOperation.h"

#include "PCGExTensorInertia.generated.h"


USTRUCT(BlueprintType)
struct FPCGExTensorInertiaConfig : public FPCGExTensorConfigBase
{
	GENERATED_BODY()

	FPCGExTensorInertiaConfig() :
		FPCGExTensorConfigBase(true, false)
	{
	}

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExAxis Axis = EPCGExAxis::Forward;

	/** If enabled, will set a constant per-point inertia based on the original point transform */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bSetInertiaOnce = false;
};

/**
 * 
 */
UCLASS(MinimalAPI)
class UPCGExTensorInertia : public UPCGExTensorPointOperation
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExTensorInertiaConfig Config;

	virtual bool Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory) override;

	virtual PCGExTensor::FTensorSample Sample(int32 InSeedIndex, const FTransform& InProbe) const override;
};


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExTensorInertiaFactory : public UPCGExTensorPointFactoryData
{
	GENERATED_BODY()

public:
	FPCGExTensorInertiaConfig Config;
	virtual UPCGExTensorOperation* CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Tensors|Params")
class UPCGExCreateTensorInertiaSettings : public UPCGExTensorPointFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(TensorInertia, "Tensor : Inertia", "A tensor constant that uses the seed transform.")

#endif
	//~End UPCGSettings

	/** Tensor properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExTensorInertiaConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

protected:
	virtual bool IsCacheable() const override { return true; }
};
