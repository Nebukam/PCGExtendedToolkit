// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Samplers/PCGExTensorSampler.h"

#include "PCGExTensorHandler.generated.h"

namespace PCGExTensor
{
	struct FTensorSample;
}

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTensorSamplerDetails
{
	GENERATED_BODY()

	FPCGExTensorSamplerDetails()
	{
	}

	virtual ~FPCGExTensorSamplerDetails()
	{
	}

	/** Sampler type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	TSubclassOf<UPCGExTensorSampler> Sampler = UPCGExTensorSampler::StaticClass();

	/** Sampling radius. Whether it has any effect depends on the selected sampler. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	double Radius = 0;
};


USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTensorHandlerDetails
{
	GENERATED_BODY()

	FPCGExTensorHandlerDetails()
	{
	}

	virtual ~FPCGExTensorHandlerDetails()
	{
	}

	/** If enabled, sampling direction will be inverted. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	bool bInvert = false;

	/** If enabled, normalize sampling. This effectively negates the influence of effectors potency. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	bool bNormalize = false;

	/** Constant size applied after normalization. This will be scaled */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, DisplayName = " └─ Size", EditCondition="bNormalize", EditConditionHides))
	double SizeConstant = 1;

	/** Uniform scale factor applied to sampling after all other mutations are accounted for. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	double UniformScale = 1;

	/** Uniform scale factor applied to sampling after all other mutations are accounted for. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	FPCGExTensorSamplerDetails SamplerSettings;
};

namespace PCGExTensor
{
	class FTensorsHandler : public TSharedFromThis<FTensorsHandler>
	{
		TArray<UPCGExTensorOperation*> Tensors;
		FPCGExTensorHandlerDetails Config;

		UPCGExTensorSampler* SamplerInstance = nullptr;

	public:
		explicit FTensorsHandler(const FPCGExTensorHandlerDetails& InConfig);

		bool Init(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExTensorFactoryData>>& InFactories, const TSharedPtr<PCGExData::FFacade>& InDataFacade);
		bool Init(FPCGExContext* InContext, const FName InPin, const TSharedPtr<PCGExData::FFacade>& InDataFacade);

		FTensorSample Sample(const FTransform& InProbe, bool& OutSuccess) const;
	};
}
