// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/Tensors/PCGExTensorPathFlow.h"

#define LOCTEXT_NAMESPACE "PCGExCreateTensorPathFlow"
#define PCGEX_NAMESPACE CreateTensorPathFlow

void FPCGExTensorPathFlowConfig::Init()
{
	FPCGExTensorConfigBase::Init();
	ClosedLoop.Init();
}

bool UPCGExTensorPathFlow::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	if (!Super::Init(InContext, InFactory)) { return false; }
	return true;
}

PCGExTensor::FTensorSample UPCGExTensorPathFlow::SampleAtPosition(const FVector& InPosition) const
{
	const FBoxCenterAndExtent BCAE = FBoxCenterAndExtent(InPosition, FVector::One());

	PCGExTensor::FEffectorSamples Samples = PCGExTensor::FEffectorSamples();

	for (const TSharedPtr<const FPCGSplineStruct>& Spline : *Splines)
	{
		FTransform T = FTransform::Identity;
		double Factor = 0;
		FVector Guide = FVector::ZeroVector;

		if (!ComputeFactor(InPosition, *Spline.Get(), Config.Radius, T, Factor, Guide)) { continue; }

		Samples.Emplace_GetRef(
			FRotationMatrix::MakeFromX(PCGExMath::GetDirection(T.GetRotation(), Config.SplineDirection)).ToQuat().RotateVector(Guide),
			Config.Potency * Config.PotencyFalloffCurveObj->Eval(Factor),
			Config.Weight * Config.WeightFalloffCurveObj->Eval(Factor));
	}

	return Samples.Flatten(Config.TensorWeight);
}

PCGEX_TENSOR_BOILERPLATE(
	PathFlow, {
	NewFactory->Config.Potency *=NewFactory->Config.PotencyScale;
	NewFactory->bBuildFromPaths = GetBuildFromPoints();
	NewFactory->PointType = Config.PointType;
	NewFactory->ClosedLoop = Config.ClosedLoop;
	}, {
	NewOperation->Splines = &ManagedSplines;
	})

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
