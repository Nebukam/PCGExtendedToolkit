// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/Tensors/PCGExTensor.h"
#include "Transform/Tensors/PCGExTensorOperation.h"

PCGExTensor::FTensorSample FPCGExTensorSamplingMutationsDetails::Mutate(const FTransform& InProbe, PCGExTensor::FTensorSample InSample) const
{
	if (bInvert) { InSample.DirectionAndSize *= -1; }

	if (bBidirectional)
	{
		if (FVector::DotProduct(
			PCGExMath::GetDirection(InProbe.GetRotation(), BidirectionalAxisReference),
			InSample.DirectionAndSize.GetSafeNormal()) < 0)
		{
			InSample.DirectionAndSize = InSample.DirectionAndSize * -1;
			InSample.Rotation = FQuat(-InSample.Rotation.X, -InSample.Rotation.Y, -InSample.Rotation.Y, InSample.Rotation.W);
		}
	}

	return InSample;
}

void FPCGExTensorConfigBase::Init()
{
	PCGEX_MAKE_SHARED(CurvePaths, TSet<FSoftObjectPath>)

	if (!bUseLocalWeightFalloffCurve) { CurvePaths->Add(WeightFalloffCurve.ToSoftObjectPath()); }
	if (!bUseLocalPotencyFalloffCurve) { CurvePaths->Add(PotencyFalloffCurve.ToSoftObjectPath()); }
	if (!bUseLocalGuideCurve) { CurvePaths->Add(GuideCurve.ToSoftObjectPath()); }

	if (!CurvePaths->IsEmpty()) { PCGExHelpers::LoadBlocking_AnyThread(CurvePaths); }

	LocalWeightFalloffCurve.ExternalCurve = WeightFalloffCurve.Get();
	WeightFalloffCurveObj = LocalWeightFalloffCurve.GetRichCurveConst();

	LocalPotencyFalloffCurve.ExternalCurve = PotencyFalloffCurve.Get();
	PotencyFalloffCurveObj = LocalPotencyFalloffCurve.GetRichCurveConst();

	LocalGuideCurve.ExternalCurve = GuideCurve.Get();
}

namespace PCGExTensor
{
	FEffectorSample& FEffectorSamples::Emplace_GetRef(const FVector& InDirection, const double InPotency, const double InWeight)
	{
		TotalPotency += InPotency;
		TensorSample.Weight += InWeight;
		return Samples.Emplace_GetRef(InDirection, InPotency, InWeight);
	}
}
