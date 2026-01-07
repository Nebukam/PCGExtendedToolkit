// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Tensors/PCGExTensorPathFlow.h"

#include "Containers/PCGExManagedObjects.h"


#define LOCTEXT_NAMESPACE "PCGExCreateTensorPathFlow"
#define PCGEX_NAMESPACE CreateTensorPathFlow

bool FPCGExTensorPathFlow::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	if (!PCGExTensorOperation::Init(InContext, InFactory)) { return false; }
	return true;
}

PCGExTensor::FTensorSample FPCGExTensorPathFlow::Sample(const int32 InSeedIndex, const FTransform& InProbe) const
{
	const FVector& InPosition = InProbe.GetLocation();
	PCGExTensor::FEffectorSamples Samples = PCGExTensor::FEffectorSamples();

	for (const TSharedPtr<const FPCGSplineStruct>& Spline : *Splines)
	{
		FTransform T = FTransform::Identity;
		PCGExTensor::FEffectorMetrics Metrics;
		if (!ComputeFactor(InPosition, *Spline.Get(), Config.Radius, T, Metrics)) { continue; }

		Samples.Emplace_GetRef(FRotationMatrix::MakeFromX(PCGExMath::GetDirection(T.GetRotation(), Config.SplineDirection)).ToQuat().RotateVector(Metrics.Guide), Metrics.Potency, Metrics.Weight);
	}

	return Config.Mutations.Mutate(InProbe, Samples.Flatten(Config.TensorWeight));
}

PCGEX_TENSOR_BOILERPLATE(
	PathFlow,
	{
	NewFactory->Config.Potency *= NewFactory->Config.PotencyScale;
	NewFactory->bBuildFromPaths = GetBuildFromPoints();
	NewFactory->PointType = NewFactory->Config.PointType;
	NewFactory->bSmoothLinear = NewFactory->Config.bSmoothLinear;
	},
	{
	NewOperation->Splines = &ManagedSplines;
	})

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
