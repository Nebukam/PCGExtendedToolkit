// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/Tensors/PCGExTensorMagnet.h"

#define LOCTEXT_NAMESPACE "PCGExCreateTensorMagnet"
#define PCGEX_NAMESPACE CreateTensorMagnet

bool UPCGExTensorMagnet::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	if (!Super::Init(InContext, InFactory)) { return false; }
	return true;
}

PCGExTensor::FTensorSample UPCGExTensorMagnet::SampleAtPosition(const FVector& InPosition) const
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
			(InPosition - Center).GetSafeNormal(),
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

PCGEX_TENSOR_BOILERPLATE(Magnet)

bool UPCGExTensorMagnetFactory::InitInternalData(FPCGExContext* InContext)
{
	if (!Super::InitInternalData(InContext)) { return false; }

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
