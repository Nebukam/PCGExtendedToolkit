// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Core/Samplers/PCGExTensorSampler.h"
#include "Details/PCGExSettingsDetails.h"

#include "PCGExTensorHandler.generated.h"

class UPCGExTensorFactoryData;

namespace PCGExTensor
{
	struct FTensorSample;
}

USTRUCT(BlueprintType)
struct PCGEXELEMENTSTENSORS_API FPCGExTensorSamplerDetails
{
	GENERATED_BODY()

	FPCGExTensorSamplerDetails()
	{
	}

	virtual ~FPCGExTensorSamplerDetails()
	{
	}

	/** Sampler type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PCG_Overridable))
	TSubclassOf<UPCGExTensorSampler> Sampler = UPCGExTensorSampler::StaticClass();

	/** Sampling radius. Whether it has any effect depends on the selected sampler. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double Radius = 0;
};


USTRUCT(BlueprintType)
struct PCGEXELEMENTSTENSORS_API FPCGExTensorHandlerDetails
{
	GENERATED_BODY()

	FPCGExTensorHandlerDetails()
	{
		SizeAttribute.Update("ExtrusionSize");
	}

	virtual ~FPCGExTensorHandlerDetails()
	{
	}

	/** If enabled, sampling direction will be inverted. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bInvert = false;

	/** If enabled, normalize sampling. This effectively negates the influence of effectors potency. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bNormalize = true;

	/** Type of Size */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName=" ├─ Size Input"))
	EPCGExInputValueType SizeInput = EPCGExInputValueType::Constant;

	/** Start Offset Attribute (Vector 2 expected)*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Size (Attr)", EditCondition="bNormalize && SizeInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector SizeAttribute;

	/** Constant size applied after normalization. This will be scaled */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName = " └─ Size", EditCondition="bNormalize && SizeInput == EPCGExInputValueType::Constant", EditConditionHides))
	double SizeConstant = 100;

	PCGEX_SETTING_VALUE_DECL(Size, double)

	/** Uniform scale factor applied to sampling after all other mutations are accounted for. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double UniformScale = 1;

	/** Uniform scale factor applied to sampling after all other mutations are accounted for. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExTensorSamplerDetails SamplerSettings;
};

namespace PCGExTensor
{
	class FTensorsHandler : public TSharedFromThis<FTensorsHandler>
	{
		TArray<TSharedPtr<PCGExTensorOperation>> Tensors;
		FPCGExTensorHandlerDetails Config;
		TSharedPtr<PCGExDetails::TSettingValue<double>> Size;

		UPCGExTensorSampler* SamplerInstance = nullptr;

	public:
		explicit FTensorsHandler(const FPCGExTensorHandlerDetails& InConfig);

		bool Init(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExTensorFactoryData>>& InFactories, const TSharedPtr<PCGExData::FFacade>& InDataFacade);
		bool Init(FPCGExContext* InContext, const FName InPin, const TSharedPtr<PCGExData::FFacade>& InDataFacade);

		FTensorSample Sample(int32 InSeedIndex, const FTransform& InProbe, bool& OutSuccess) const;
	};
}
