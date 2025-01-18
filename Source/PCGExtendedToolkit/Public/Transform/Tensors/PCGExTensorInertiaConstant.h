// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "PCGExTensor.h"
#include "PCGExTensorFactoryProvider.h"
#include "PCGExTensorOperation.h"

#include "PCGExTensorInertiaConstant.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTensorInertiaConstantConfig : public FPCGExTensorConfigBase
{
	GENERATED_BODY()

	FPCGExTensorInertiaConstantConfig() :
		FPCGExTensorConfigBase(true, false)
	{
	}

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExAxis Axis = EPCGExAxis::Forward;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FRotator Offset = FRotator::ZeroRotator;
};

/**
 * 
 */
UCLASS(MinimalAPI)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTensorInertiaConstant : public UPCGExTensorOperation
{
	GENERATED_BODY()

public:
	FPCGExTensorInertiaConstantConfig Config;
	FQuat Offset = FQuat::Identity;
	virtual bool Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory) override;

	virtual PCGExTensor::FTensorSample Sample(const FTransform& InProbe) const override;
};


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTensorInertiaConstantFactory : public UPCGExTensorFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExTensorInertiaConstantConfig Config;

	virtual UPCGExTensorOperation* CreateOperation(FPCGExContext* InContext) const override;

protected:
	virtual bool InitInternalData(FPCGExContext* InContext) override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Tensors|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExCreateTensorInertiaConstantSettings : public UPCGExTensorFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(TensorInertiaConstant, "Tensor : Inertia (Constant)", "A tensor constant that uses the seed transform.")

#endif
	//~End UPCGSettings

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExAxis Axis = EPCGExAxis::Forward;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FRotator Offset = FRotator::ZeroRotator;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double TensorWeight = 1;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double Potency = 1;

	/** Tensor properties */
	UPROPERTY(meta=(PCG_NotOverridable, HideInDetailPanel))
	FPCGExTensorInertiaConstantConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

protected:
	virtual bool IsCacheable() const override { return true; }
};
