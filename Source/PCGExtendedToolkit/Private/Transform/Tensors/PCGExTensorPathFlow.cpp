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
		const FTransform T = PCGExPaths::GetClosestTransform(Spline, InPosition, true);

		const FVector Center = T.GetLocation();
		const FVector S = T.GetScale3D();

		const double RadiusSquared = FMath::Square(FVector2D(S.Y, S.Z).Length() * Config.Radius);
		const double DistSquared = FVector::DistSquared(InPosition, Center);

		if (DistSquared > RadiusSquared) { continue; }

		const double Factor = DistSquared / RadiusSquared;

		Samples.Emplace_GetRef(
			PCGExMath::GetDirection(T.GetRotation(), Config.SplineDirection),
			Config.Potency * Config.PotencyFalloffCurveObj->Eval(Factor),
			Config.Weight * Config.WeightFalloffCurveObj->Eval(Factor));
	}

	return Samples.Flatten(Config.TensorWeight);
}

PCGEX_TENSOR_BOILERPLATE(
	PathFlow, {
		NewFactory->bBuildFromPaths = GetBuildFromPoints();
		NewFactory->PointType = Config.PointType;
		NewFactory->ClosedLoop = Config.ClosedLoop;
	}, {
	NewOperation->Splines = &ManagedSplines;
	})

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
