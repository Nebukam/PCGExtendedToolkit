// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/Tensors/PCGExTensorInertia.h"

#define LOCTEXT_NAMESPACE "PCGExCreateTensorInertia"
#define PCGEX_NAMESPACE CreateTensorInertia

bool UPCGExTensorInertia::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	if (!Super::Init(InContext, InFactory)) { return false; }
	return true;
}

PCGExTensor::FTensorSample UPCGExTensorInertia::Sample(const FTransform& InProbe) const
{
	const FVector& InPosition = InProbe.GetLocation();
	const FBoxCenterAndExtent BCAE = FBoxCenterAndExtent(InPosition, FVector::One());

	PCGExTensor::FEffectorSamples Samples = PCGExTensor::FEffectorSamples();

	auto ProcessNeighbor = [&](const FPCGPointRef& InEffector)
	{
		PCGExTensor::FEffectorMetrics Metrics;
		if (!ComputeFactor(InPosition, InEffector, Metrics)) { return; }

		Samples.Emplace_GetRef(
			PCGExMath::GetDirection(InProbe.GetRotation() * FRotationMatrix::MakeFromX(Metrics.Guide).ToQuat(), Config.Axis),
			Metrics.Potency, Metrics.Weight);
	};

	Octree->FindElementsWithBoundsTest(BCAE, ProcessNeighbor);

	return Samples.Flatten(Config.TensorWeight);
}

PCGEX_TENSOR_BOILERPLATE(Inertia, {}, {})

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
