// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/Tensors/PCGExTensorFlow.h"

#define LOCTEXT_NAMESPACE "PCGExCreateTensorFlow"
#define PCGEX_NAMESPACE CreateTensorFlow

bool UPCGExTensorFlow::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	if (!Super::Init(InContext, InFactory)) { return false; }
	return true;
}

PCGExTensor::FTensorSample UPCGExTensorFlow::SampleAtPosition(const FVector& InPosition) const
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

	PCGExTensor::FTensorSample Result = Samples.Flatten(Config.TensorWeight);

	// Remove rotation & adjust translation
	const FVector Translation = Result.Transform.TransformPosition(FVector::ZeroVector);
	Result.Transform.SetLocation(Translation);
	Result.Transform.SetRotation(FQuat::Identity);

	return Result;
}

PCGEX_TENSOR_BOILERPLATE(Flow)

bool UPCGExTensorFlowFactory::InitInternalData(FPCGExContext* InContext)
{
	if (!Super::InitInternalData(InContext)) { return false; }

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
