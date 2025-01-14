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
	FTensorSample FTensorSample::operator+(const FTensorSample& Other) const
	{
		return FTensorSample(
			DirectionAndSize + Other.DirectionAndSize,
			Rotation * Other.Rotation,
			Effectors + Other.Effectors,
			Weight + Weight);
	}

	FTensorSample& FTensorSample::operator+=(const FTensorSample& Other)
	{
		DirectionAndSize += Other.DirectionAndSize;
		Rotation *= Other.Rotation;
		Effectors += Other.Effectors;
		Weight += Weight;
		return *this;
	}

	FTensorSample FTensorSample::operator*(const double Factor) const
	{
		return FTensorSample(
			DirectionAndSize * Factor,
			Rotation * Factor,
			Effectors,
			Weight * Factor);
	}

	FTensorSample& FTensorSample::operator*=(const double Factor)
	{
		DirectionAndSize *= Factor;
		Rotation *= Factor;
		Weight *= Weight;
		return *this;
	}

	FTensorSample FTensorSample::operator/(const double Factor) const
	{
		const double Divisor = 1 / Factor;
		return FTensorSample(
			DirectionAndSize * Divisor,
			Rotation * Divisor,
			Effectors,
			Weight * Divisor);
	}

	FTensorSample& FTensorSample::operator/=(const double Factor)
	{
		const double Divisor = 1 / Factor;
		DirectionAndSize *= Divisor;
		Rotation *= Divisor;
		Weight *= Divisor;
		return *this;
	}

	void FTensorSample::Transform(FTransform& InTransform, const double InWeight) const
	{
		const FVector Location = InTransform.GetLocation() + DirectionAndSize * InWeight;
		InTransform.SetRotation((InTransform.GetRotation() * (Rotation * InWeight)).GetNormalized());
		InTransform.SetLocation(Location);
	}

	FTransform FTensorSample::GetTransformed(const FTransform& InTransform, const double InWeight) const
	{
		return FTransform(
			(InTransform.GetRotation() * (Rotation * InWeight)).GetNormalized(),
			InTransform.GetLocation() + DirectionAndSize * InWeight,
			InTransform.GetScale3D());
	}
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
