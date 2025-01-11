// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/Tensors/PCGExTensorSplinePole.h"

#define LOCTEXT_NAMESPACE "PCGExCreateTensorSplinePole"
#define PCGEX_NAMESPACE CreateTensorSplinePole

bool UPCGExTensorSplinePole::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	if (!Super::Init(InContext, InFactory)) { return false; }
	return true;
}

PCGExTensor::FTensorSample UPCGExTensorSplinePole::SampleAtPosition(const FVector& InPosition) const
{
	const FBoxCenterAndExtent BCAE = FBoxCenterAndExtent(InPosition, FVector::One());

	PCGExTensor::FEffectorSamples Samples = PCGExTensor::FEffectorSamples();

	for (const FPCGSplineStruct& Spline : *Splines)
	{
		FTransform T = FTransform::Identity;
		PCGExTensor::FEffectorMetrics Metrics;

		if (!ComputeFactor(InPosition, Spline, Config.Radius, T, Metrics)) { continue; }

		Samples.Emplace_GetRef(
			FRotationMatrix::MakeFromX((InPosition - T.GetLocation()).GetSafeNormal()).ToQuat().RotateVector(Metrics.Guide),
			Metrics.Potency, Metrics.Weight);
	}

	return Samples.Flatten(Config.TensorWeight);
}

bool UPCGExTensorSplinePoleFactory::Prepare(FPCGExContext* InContext)
{
	SampleInputs = Config.SampleInputs;
	return Super::Prepare(InContext);
}

PCGEX_TENSOR_BOILERPLATE(
	SplinePole, {
	NewFactory->Config.Potency *=NewFactory->Config.PotencyScale;
	}, {
	NewOperation->Splines = &Splines;
	})

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
