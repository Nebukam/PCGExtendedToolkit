// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/Tensors/PCGExTensorPathPole.h"

#define LOCTEXT_NAMESPACE "PCGExCreateTensorPathPole"
#define PCGEX_NAMESPACE CreateTensorPathPole

bool UPCGExTensorPathPole::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	if (!Super::Init(InContext, InFactory)) { return false; }
	return true;
}

PCGExTensor::FTensorSample UPCGExTensorPathPole::SampleAtPosition(const FVector& InPosition) const
{
	const FBoxCenterAndExtent BCAE = FBoxCenterAndExtent(InPosition, FVector::One());

	PCGExTensor::FEffectorSamples Samples = PCGExTensor::FEffectorSamples();

	for (const TSharedPtr<const FPCGSplineStruct>& Spline : *Splines)
	{
		FTransform T = FTransform::Identity;
		PCGExTensor::FEffectorMetrics Metrics;

		if (!ComputeFactor(InPosition, *Spline.Get(), Config.Radius, T, Metrics)) { continue; }

		Samples.Emplace_GetRef(
			FRotationMatrix::MakeFromX((InPosition - T.GetLocation()).GetSafeNormal()).ToQuat().RotateVector(Metrics.Guide),
			Metrics.Potency, Metrics.Weight);
	}

	return Samples.Flatten(Config.TensorWeight);
}

PCGEX_TENSOR_BOILERPLATE(
	PathPole, {
	NewFactory->Config.Potency *=NewFactory->Config.PotencyScale;
	NewFactory->bBuildFromPaths = GetBuildFromPoints();
	NewFactory->PointType = NewFactory->Config.PointType;
	NewFactory->ClosedLoop = NewFactory->Config.ClosedLoop;
	}, {
	NewOperation->Splines = &ManagedSplines;
	})

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
