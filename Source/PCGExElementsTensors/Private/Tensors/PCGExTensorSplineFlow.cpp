// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Tensors/PCGExTensorSplineFlow.h"

#include "Data/PCGSplineStruct.h"
#include "Containers/PCGExManagedObjects.h"


#define LOCTEXT_NAMESPACE "PCGExCreateTensorSplineFlow"
#define PCGEX_NAMESPACE CreateTensorSplineFlow

bool FPCGExTensorSplineFlow::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	if (!PCGExTensorOperation::Init(InContext, InFactory)) { return false; }
	return true;
}

PCGExTensor::FTensorSample FPCGExTensorSplineFlow::Sample(const int32 InSeedIndex, const FTransform& InProbe) const
{
	const FVector& InPosition = InProbe.GetLocation();
	PCGExTensor::FEffectorSamples Samples = PCGExTensor::FEffectorSamples();

	for (const FPCGSplineStruct& Spline : *Splines)
	{
		FTransform T = FTransform::Identity;
		PCGExTensor::FEffectorMetrics Metrics;

		if (!ComputeFactor(InPosition, Spline, Config.Radius, T, Metrics)) { continue; }

		Samples.Emplace_GetRef(FRotationMatrix::MakeFromX(PCGExMath::GetDirection(T.GetRotation(), Config.SplineDirection)).ToQuat().RotateVector(Metrics.Guide), Metrics.Potency, Metrics.Weight);
	}

	return Config.Mutations.Mutate(InProbe, Samples.Flatten(Config.TensorWeight));
}

PCGExFactories::EPreparationResult UPCGExTensorSplineFlowFactory::Prepare(FPCGExContext* InContext, const TSharedPtr<PCGExMT::FTaskManager>& TaskManager)
{
	SampleInputs = Config.SampleInputs;
	return Super::Prepare(InContext, TaskManager);
}

PCGEX_TENSOR_BOILERPLATE(
	SplineFlow,
	{
	NewFactory->Config.Potency *=NewFactory->Config.PotencyScale;
	},
	{
	NewOperation->Splines = &Splines;
	})

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
