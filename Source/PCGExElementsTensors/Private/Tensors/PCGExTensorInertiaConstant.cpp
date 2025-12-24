// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Tensors/PCGExTensorInertiaConstant.h"


#include "Containers/PCGExManagedObjects.h"
#include "Data/PCGExData.h"


#define LOCTEXT_NAMESPACE "PCGExCreateTensorInertiaConstant"
#define PCGEX_NAMESPACE CreateTensorInertiaConstant

bool FPCGExTensorInertiaConstant::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	if (!PCGExTensorOperation::Init(InContext, InFactory)) { return false; }
	Offset = Config.Offset.Quaternion();
	return true;
}

PCGExTensor::FTensorSample FPCGExTensorInertiaConstant::Sample(const int32 InSeedIndex, const FTransform& InProbe) const
{
	PCGExTensor::FEffectorSamples Samples = PCGExTensor::FEffectorSamples();

	if (Config.bSetInertiaOnce)
	{
		Samples.Emplace_GetRef(PCGExMath::GetDirection(PrimaryDataFacade->GetIn()->GetTransform(InSeedIndex).GetRotation() * Offset, Config.Axis), Config.Potency, Config.Weight);
	}
	else
	{
		Samples.Emplace_GetRef(PCGExMath::GetDirection(InProbe.GetRotation() * Offset, Config.Axis), Config.Potency, Config.Weight);
	}


	return Samples.Flatten(Config.TensorWeight);
}

PCGEX_TENSOR_BOILERPLATE(
	InertiaConstant,
	{
	NewFactory->Config.Axis = Axis;
	NewFactory->Config.Offset = Offset;
	NewFactory->Config.Potency = Potency;
	NewFactory->Config.PotencyInput = EPCGExInputValueType::Constant;
	NewFactory->Config.Weight = 1;
	NewFactory->Config.TensorWeight = TensorWeight;
	NewFactory->Config.WeightInput = EPCGExInputValueType::Constant;
	NewFactory->Config.bSetInertiaOnce = bSetInertiaOnce;
	},
	{})

PCGExFactories::EPreparationResult UPCGExTensorInertiaConstantFactory::InitInternalData(FPCGExContext* InContext)
{
	if (Config.PotencyInput == EPCGExInputValueType::Attribute)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Attribute-driven Potency is not supported on Constant Tensor."));
		return PCGExFactories::EPreparationResult::Fail;
	}

	if (Config.WeightInput == EPCGExInputValueType::Attribute)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Attribute-driven Weight is not supported on Constant Tensor."));
		return PCGExFactories::EPreparationResult::Fail;
	}

	return Super::InitInternalData(InContext);
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
