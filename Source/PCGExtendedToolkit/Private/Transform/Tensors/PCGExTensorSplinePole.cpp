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
		const FTransform T = PCGExPaths::GetClosestTransform(Spline, InPosition, true);

		const FVector Center = T.GetLocation();
		const FVector S = T.GetScale3D();

		const double RadiusSquared = FMath::Square(FVector2D(S.Y, S.Z).Length() * Config.Radius);
		const double DistSquared = FVector::DistSquared(InPosition, Center);

		if (DistSquared > RadiusSquared) { continue; }

		const double Factor = DistSquared / RadiusSquared;

		Samples.Emplace_GetRef(
			(InPosition - Center).GetSafeNormal(),
			Config.Potency * Config.PotencyFalloffCurveObj->Eval(Factor),
			Config.Weight * Config.WeightFalloffCurveObj->Eval(Factor));
	}

	return Samples.Flatten(Config.TensorWeight);
}

bool UPCGExTensorSplinePoleFactory::Prepare(FPCGExContext* InContext)
{
	SampleInputs = Config.SampleInputs;
	return Super::Prepare(InContext);
}

PCGEX_TENSOR_BOILERPLATE(
	SplinePole, { }, {
	NewOperation->Splines = &Splines;
	})

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
