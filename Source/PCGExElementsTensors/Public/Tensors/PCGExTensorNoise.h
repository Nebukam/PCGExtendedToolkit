// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExNoise3DCommon.h"
#include "Core/PCGExTensor.h"
#include "Core/PCGExTensorFactoryProvider.h"
#include "Core/PCGExTensorOperation.h"

#include "PCGExTensorNoise.generated.h"

namespace PCGExNoise3D
{
	class FNoiseGenerator;
}

USTRUCT(BlueprintType)
struct FPCGExTensorNoiseConfig : public FPCGExTensorConfigBase
{
	GENERATED_BODY()

	FPCGExTensorNoiseConfig()
		: FPCGExTensorConfigBase()
	{
	}
};

/**
 * 
 */
class FPCGExTensorNoise : public PCGExTensorOperation
{
public:
	FPCGExTensorNoiseConfig Config;
	TSharedPtr<PCGExNoise3D::FNoiseGenerator> NoiseGenerator = nullptr;
	
	virtual bool Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory) override;

	virtual PCGExTensor::FTensorSample Sample(int32 InSeedIndex, const FTransform& InProbe) const override;
};


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExTensorNoiseFactory : public UPCGExTensorFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExTensorNoiseConfig Config;

	UPROPERTY()
	FVector Noise = FVector::OneVector;
	
	TSharedPtr<PCGExNoise3D::FNoiseGenerator> NoiseGenerator = nullptr;

	virtual TSharedPtr<PCGExTensorOperation> CreateOperation(FPCGExContext* InContext) const override;

protected:
	virtual PCGExFactories::EPreparationResult InitInternalData(FPCGExContext* InContext) override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Tensors|Params", meta=(PCGExNodeLibraryDoc="tensors/effectors/tensor-noise"))
class UPCGExCreateTensorNoiseSettings : public UPCGExTensorFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(TensorNoise, "Tensor : Noise", "A tensor that has a constant value in the field. Note that this tensor will prevent sampling from failing.")

#endif
	//~End UPCGSettings

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	
public:
	
	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double TensorWeight = 1;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double Potency = 1;

	/** Tensor mutations settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Sampling Mutations"))
	FPCGExTensorSamplingMutationsDetails Mutations;

	/** Tensor properties */
	UPROPERTY(meta=(PCG_NotOverridable, HideInDetailPanel))
	FPCGExTensorNoiseConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;
};
