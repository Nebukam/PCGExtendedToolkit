// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Tensors/PCGExTensorPathPole.h"

#include "Containers/PCGExManagedObjects.h"


#define LOCTEXT_NAMESPACE "PCGExCreateTensorPathPole"
#define PCGEX_NAMESPACE CreateTensorPathPole

PCGExTensor::FTensorSample FPCGExTensorPathPole::Sample(const int32 InSeedIndex, const FTransform& InProbe) const
{
	const FVector& InPosition = InProbe.GetLocation();
	PCGExTensor::FEffectorSamples Samples = PCGExTensor::FEffectorSamples();

	for (const TSharedPtr<const FPCGSplineStruct>& Spline : *Splines)
	{
		FTransform T = FTransform::Identity;
		PCGExTensor::FEffectorMetrics Metrics;

		if (!ComputeFactor(InPosition, *Spline.Get(), Config.Radius, T, Metrics)) { continue; }

		Samples.Emplace_GetRef(FRotationMatrix::MakeFromX((InPosition - T.GetLocation()).GetSafeNormal()).ToQuat().RotateVector(Metrics.Guide), Metrics.Potency, Metrics.Weight);
	}

	return Config.Mutations.Mutate(InProbe, Samples.Flatten(Config.TensorWeight));
}

PCGEX_TENSOR_BOILERPLATE(
	PathPole,
	{
	NewFactory->Config.Potency *=NewFactory->Config.PotencyScale;
	NewFactory->bBuildFromPaths = GetBuildFromPoints();
	NewFactory->PointType = NewFactory->Config.PointType;
	NewFactory->bSmoothLinear = NewFactory->Config.bSmoothLinear;
	},
	{
	NewOperation->Splines = &ManagedSplines;
	})

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
