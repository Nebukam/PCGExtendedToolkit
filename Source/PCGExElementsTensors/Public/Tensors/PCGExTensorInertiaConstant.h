// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExTensor.h"
#include "Core/PCGExTensorFactoryProvider.h"
#include "Core/PCGExTensorOperation.h"
#include "Math/PCGExMathAxis.h"
#include "PCGExTensorInertiaConstant.generated.h"

USTRUCT(BlueprintType)
struct FPCGExTensorInertiaConstantConfig : public FPCGExTensorConfigBase
{
	GENERATED_BODY()

	FPCGExTensorInertiaConstantConfig()
		: FPCGExTensorConfigBase(true, false)
	{
	}

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExAxis Axis = EPCGExAxis::Forward;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FRotator Offset = FRotator::ZeroRotator;

	/** If enabled, will set a constant per-point inertia based on the original point transform */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bSetInertiaOnce = false;
};

/**
 * 
 */
class FPCGExTensorInertiaConstant : public PCGExTensorOperation
{
public:
	FPCGExTensorInertiaConstantConfig Config;
	FQuat Offset = FQuat::Identity;
	virtual bool Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory) override;

	virtual PCGExTensor::FTensorSample Sample(int32 InSeedIndex, const FTransform& InProbe) const override;
};


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExTensorInertiaConstantFactory : public UPCGExTensorFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExTensorInertiaConstantConfig Config;

	virtual TSharedPtr<PCGExTensorOperation> CreateOperation(FPCGExContext* InContext) const override;

protected:
	virtual PCGExFactories::EPreparationResult InitInternalData(FPCGExContext* InContext) override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Tensors|Params", meta=(PCGExNodeLibraryDoc="tensors/effectors/tensor-inertia-constant"))
class UPCGExCreateTensorInertiaConstantSettings : public UPCGExTensorFactoryProviderSettings
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

	/** If enabled, will set a constant per-point inertia based on the original point transform */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bSetInertiaOnce = false;

	/** Tensor properties */
	UPROPERTY(meta=(PCG_NotOverridable, HideInDetailPanel))
	FPCGExTensorInertiaConstantConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;
};
