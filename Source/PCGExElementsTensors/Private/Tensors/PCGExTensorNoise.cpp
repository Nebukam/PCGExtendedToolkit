// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Tensors/PCGExTensorNoise.h"

#include "Containers/PCGExManagedObjects.h"
#include "Core/PCGExNoise3DCommon.h"
#include "Core/PCGExNoise3DFactoryProvider.h"
#include "Core/PCGExTensorOperation.h"
#include "Helpers/PCGExNoiseGenerator.h"

#define LOCTEXT_NAMESPACE "PCGExCreateTensorNoise"
#define PCGEX_NAMESPACE CreateTensorNoise

bool FPCGExTensorNoise::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	if (!PCGExTensorOperation::Init(InContext, InFactory)) { return false; }
	return true;
}

PCGExTensor::FTensorSample FPCGExTensorNoise::Sample(const int32 InSeedIndex, const FTransform& InProbe) const
{
	PCGExTensor::FEffectorSamples Samples = PCGExTensor::FEffectorSamples();

	Samples.Emplace_GetRef(NoiseGenerator->GetVector(InProbe.GetLocation()).GetSafeNormal(), Config.Potency, Config.Weight);

	return Config.Mutations.Mutate(InProbe, Samples.Flatten(Config.TensorWeight));
}

PCGEX_TENSOR_BOILERPLATE(
	Noise,
	{
	NewFactory->Config.Mutations = Mutations;
	NewFactory->Config.Potency = Potency;
	NewFactory->Config.PotencyInput = EPCGExInputValueType::Constant;
	NewFactory->Config.Weight = 1;
	NewFactory->Config.TensorWeight = TensorWeight;
	NewFactory->Config.WeightInput = EPCGExInputValueType::Constant;
	NewFactory->NoiseGenerator = MakeShared<PCGExNoise3D::FNoiseGenerator>();
	if (!NewFactory->NoiseGenerator->Init(InContext)) { return nullptr; }
	},
	{
		NewOperation->NoiseGenerator = NoiseGenerator;
	})

PCGExFactories::EPreparationResult UPCGExTensorNoiseFactory::InitInternalData(FPCGExContext* InContext)
{
	if (Config.PotencyInput == EPCGExInputValueType::Attribute)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Attribute-driven Potency is not supported on Noise Tensor."));
		return PCGExFactories::EPreparationResult::Fail;
	}

	if (Config.WeightInput == EPCGExInputValueType::Attribute)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Attribute-driven Weight is not supported on Noise Tensor."));
		return PCGExFactories::EPreparationResult::Fail;
	}

	return Super::InitInternalData(InContext);
}

TArray<FPCGPinProperties> UPCGExCreateTensorNoiseSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_FACTORIES(PCGExNoise3D::Labels::SourceNoise3DLabel, "Noise nodes", Required, FPCGExDataTypeInfoNoise3D::AsId())
	return PinProperties;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
