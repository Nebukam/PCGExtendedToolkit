// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Tensors/PCGExTensorPole.h"

#include "Containers/PCGExManagedObjects.h"


#define LOCTEXT_NAMESPACE "PCGExCreateTensorPole"
#define PCGEX_NAMESPACE CreateTensorPole

bool FPCGExTensorPole::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	if (!PCGExTensorPointOperation::Init(InContext, InFactory)) { return false; }
	return true;
}

PCGExTensor::FTensorSample FPCGExTensorPole::Sample(const int32 InSeedIndex, const FTransform& InProbe) const
{
	const FVector& InPosition = InProbe.GetLocation();
	const FBoxCenterAndExtent BCAE = FBoxCenterAndExtent(InPosition, FVector::One());

	PCGExTensor::FEffectorSamples Samples = PCGExTensor::FEffectorSamples();

	auto ProcessNeighbor = [&](const PCGExOctree::FItem& InEffector)
	{
		PCGExTensor::FEffectorMetrics Metrics;
		if (const auto* E = ComputeFactor(InPosition, InEffector.Index, Metrics))
		{
			Samples.Emplace_GetRef(FRotationMatrix::MakeFromX((InPosition - E->Location).GetSafeNormal()).ToQuat().RotateVector(Metrics.Guide), Metrics.Potency, Metrics.Weight);
		}
	};

	Effectors->GetOctree()->FindElementsWithBoundsTest(BCAE, ProcessNeighbor);

	return Config.Mutations.Mutate(InProbe, Samples.Flatten(Config.TensorWeight));
}

PCGEX_TENSOR_BOILERPLATE(Pole, {}, {})

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
