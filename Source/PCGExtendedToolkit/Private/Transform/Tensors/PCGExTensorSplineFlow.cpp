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

	for (const FPCGSplineStruct& Spline : *Splines)
	{
		FTransform T = FTransform::Identity;
		double Factor = 0;
		
		if (!ComputeFactor(InPosition, Spline, Config.Radius, T, Factor)) { continue; }

		Samples.Emplace_GetRef(
			PCGExMath::GetDirection(T.GetRotation(), Config.SplineDirection),
			Config.Potency * Config.PotencyFalloffCurveObj->Eval(Factor),
			Config.Weight * Config.WeightFalloffCurveObj->Eval(Factor));
	}

	return Samples.Flatten(Config.TensorWeight);
}

bool UPCGExTensorSplineFlowFactory::Prepare(FPCGExContext* InContext)
{
	SampleInputs = Config.SampleInputs;
	return Super::Prepare(InContext);
}

PCGEX_TENSOR_BOILERPLATE(
	SplineFlow, {
	NewFactory->Config.Potency *=NewFactory->Config.PotencyScale;
	}, {
	NewOperation->Splines = &Splines;
	})

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
