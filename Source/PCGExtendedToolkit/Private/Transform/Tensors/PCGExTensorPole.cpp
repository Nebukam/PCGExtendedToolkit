// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/Tensors/PCGExTensorPole.h"

#define LOCTEXT_NAMESPACE "PCGExCreateTensorPole"
#define PCGEX_NAMESPACE CreateTensorPole

bool UPCGExTensorPole::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	if (!Super::Init(InContext, InFactory)) { return false; }
	return true;
}

PCGExTensor::FTensorSample UPCGExTensorPole::SampleAtPosition(const FVector& InPosition) const
{
	const FBoxCenterAndExtent BCAE = FBoxCenterAndExtent(InPosition, FVector::One());

	PCGExTensor::FEffectorSamples Samples = PCGExTensor::FEffectorSamples();

	auto ProcessNeighbor = [&](const FPCGPointRef& InPointRef)
	{
		PCGExTensor::FEffectorMetrics Metrics;
		if (!ComputeFactor(InPosition, InPointRef, Metrics)) { return; }

		Samples.Emplace_GetRef(
			FRotationMatrix::MakeFromX((InPosition - InPointRef.Point->Transform.GetLocation()).GetSafeNormal()).ToQuat().RotateVector(Metrics.Guide),
			Metrics.Potency, Metrics.Weight);
	};

	Octree->FindElementsWithBoundsTest(BCAE, ProcessNeighbor);

	return Samples.Flatten(Config.TensorWeight);
}

PCGEX_TENSOR_BOILERPLATE(Pole, {}, {})

bool UPCGExTensorPoleFactory::InitInternalData(FPCGExContext* InContext)
{
	if (!Super::InitInternalData(InContext)) { return false; }

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
