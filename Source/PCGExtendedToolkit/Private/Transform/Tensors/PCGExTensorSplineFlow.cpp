// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/Tensors/PCGExTensorSplineFlow.h"

#define LOCTEXT_NAMESPACE "PCGExCreateTensorSplineFlow"
#define PCGEX_NAMESPACE CreateTensorSplineFlow

bool UPCGExTensorSplineFlow::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	if (!Super::Init(InContext, InFactory)) { return false; }
	return true;
}

PCGExTensor::FTensorSample UPCGExTensorSplineFlow::SampleAtPosition(const FVector& InPosition) const
{
	const FBoxCenterAndExtent BCAE = FBoxCenterAndExtent(InPosition, FVector::One());

	PCGExTensor::FEffectorSamples Samples = PCGExTensor::FEffectorSamples();

	auto ProcessNeighbor = [&](const FPCGPointRef& InPointRef)
	{
		const FVector Center = InPointRef.Point->Transform.GetLocation();
		const double RadiusSquared = InPointRef.Point->Color.W;
		const double DistSquared = FVector::DistSquared(InPosition, Center);

		if (DistSquared > RadiusSquared) { return; }

		const double Factor = DistSquared / RadiusSquared;

		Samples.Emplace_GetRef(
			PCGExMath::GetDirection(InPointRef.Point->Transform.GetRotation(), EPCGExAxis::Forward),
			InPointRef.Point->Steepness * Config.StrengthFalloffCurveObj->Eval(Factor),
			InPointRef.Point->Density * Config.WeightFalloffCurveObj->Eval(Factor));
	};

	Octree->FindElementsWithBoundsTest(BCAE, ProcessNeighbor);

	return Samples.Flatten(Config.TensorWeight);
}

bool UPCGExTensorSplineFlowFactory::Prepare(FPCGExContext* InContext)
{
	SampleInputs = Config.SampleInputs;
	return Super::Prepare(InContext);
}

PCGEX_TENSOR_BOILERPLATE(SplineFlow, {}, {})

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
