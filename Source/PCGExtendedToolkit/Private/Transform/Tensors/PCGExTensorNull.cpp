// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/Tensors/PCGExTensorNull.h"

#define LOCTEXT_NAMESPACE "PCGExCreateTensorNull"
#define PCGEX_NAMESPACE CreateTensorNull

bool UPCGExTensorNull::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	if (!Super::Init(InContext, InFactory)) { return false; }
	return true;
}

PCGExTensor::FTensorSample UPCGExTensorNull::SampleAtPosition(const FVector& InPosition) const
{
	const FBoxCenterAndExtent BCAE = FBoxCenterAndExtent(InPosition, FVector::One());

	PCGExTensor::FEffectorSamples Samples = PCGExTensor::FEffectorSamples();

	auto ProcessNeighbor = [&](const FPCGPointRef& InPointRef)
	{
		double Factor = 0;
		if (!ComputeFactor(InPosition, InPointRef, Factor)) { return; }

		Samples.Emplace_GetRef(
			FVector::ZeroVector,
			Config.Potency * Config.PotencyFalloffCurveObj->Eval(Factor) * Config.Weight * Config.WeightFalloffCurveObj->Eval(Factor),
			1);
	};

	Octree->FindElementsWithBoundsTest(BCAE, ProcessNeighbor);
	return Samples.Flatten(Samples.TotalPotency * Config.TensorWeight);
}

PCGEX_TENSOR_BOILERPLATE(Null, {}, {})

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
