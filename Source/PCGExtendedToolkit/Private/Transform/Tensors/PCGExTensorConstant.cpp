// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/Tensors/PCGExTensorConstant.h"

#define LOCTEXT_NAMESPACE "PCGExCreateTensorConstant"
#define PCGEX_NAMESPACE CreateTensorConstant

bool UPCGExTensorConstant::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	if (!Super::Init(InContext, InFactory)) { return false; }
	return true;
}

PCGExTensor::FTensorSample UPCGExTensorConstant::SampleAtPosition(const FVector& InPosition) const
{
	const FBoxCenterAndExtent BCAE = FBoxCenterAndExtent(InPosition, FVector::One());

	PCGExTensor::FEffectorSamples Samples = PCGExTensor::FEffectorSamples();

	Samples.Emplace_GetRef(
		Config.Direction,
		Config.Strength,
		Config.Weight);

	return Samples.Flatten(Config.TensorWeight);
}

PCGEX_TENSOR_BOILERPLATE(Constant, {
	NewFactory->Config.Direction = Direction;
	NewFactory->Config.Strength = Strength;
	NewFactory->Config.StrengthInput = EPCGExInputValueType::Constant;
	NewFactory->Config.Weight = Weight;
	NewFactory->Config.WeightInput = EPCGExInputValueType::Constant;
}, {})

bool UPCGExTensorConstantFactory::InitInternalData(FPCGExContext* InContext)
{
	if (Config.StrengthInput == EPCGExInputValueType::Attribute)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Attribute-driven Strength is not supported on Constant Tensor."));
		return false;
	}

	if (Config.WeightInput == EPCGExInputValueType::Attribute)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Attribute-driven Weight is not supported on Constant Tensor."));
		return false;
	}

	if (!Super::InitInternalData(InContext)) { return false; }
	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
