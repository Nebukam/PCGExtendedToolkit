// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/Tensors/PCGExTensor.h"

#include "Transform/Tensors/PCGExTensorFactoryProvider.h"
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

FPCGExTensorConfigBase::FPCGExTensorConfigBase(const bool SupportAttributes, const bool SupportMutations)
	: bSupportAttributes(SupportAttributes), bSupportMutations(SupportMutations)
{
	if (!bSupportAttributes)
	{
		PotencyInput = EPCGExInputValueType::Constant;
		WeightInput = EPCGExInputValueType::Constant;
	}

	LocalGuideCurve.VectorCurves[0].AddKey(0, 1);
	LocalGuideCurve.VectorCurves[1].AddKey(0, 0);
	LocalGuideCurve.VectorCurves[2].AddKey(0, 0);

	LocalGuideCurve.VectorCurves[0].AddKey(1, 1);
	LocalGuideCurve.VectorCurves[1].AddKey(1, 0);
	LocalGuideCurve.VectorCurves[2].AddKey(1, 0);

	LocalPotencyFalloffCurve.EditorCurveData.AddKey(1, 0);
	LocalPotencyFalloffCurve.EditorCurveData.AddKey(0, 1);

	LocalWeightFalloffCurve.EditorCurveData.AddKey(1, 0);
	LocalWeightFalloffCurve.EditorCurveData.AddKey(0, 1);

	PotencyAttribute.Update(TEXT("$Density"));
	WeightAttribute.Update(TEXT("Steepness"));
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
	bool FEffectorsArray::Init(FPCGExContext* InContext, const UPCGExTensorPointFactoryData* InFactory)
	{
		TSharedPtr<PCGExDetails::TSettingValue<double>> PotencyValue = InFactory->BaseConfig.GetValueSettingPotency(InFactory->bQuietMissingInputError);
		if (!PotencyValue->Init(InContext, InFactory->InputDataFacade, false)) { return false; }

		TSharedPtr<PCGExDetails::TSettingValue<double>> WeightValue = InFactory->BaseConfig.GetValueSettingWeight(InFactory->bQuietMissingInputError);
		if (!WeightValue->Init(InContext, InFactory->InputDataFacade, false)) { return false; }

		const TArray<FPCGPoint>& InPoints = InFactory->InputDataFacade->GetIn()->GetPoints();

		const FBox InBounds = InFactory->InputDataFacade->GetIn()->GetBounds();
		Octree = MakeShared<PCGEx::FIndexedItemOctree>(InBounds.GetCenter(), (InBounds.GetExtent() + FVector(10)).Length());

		PCGEx::InitArray(Transforms, InPoints.Num());
		PCGEx::InitArray(Radiuses, InPoints.Num());
		PCGEx::InitArray(Potencies, InPoints.Num());
		PCGEx::InitArray(Weights, InPoints.Num());

		// Pack per-point data
		for (int i = 0; i < InPoints.Num(); i++)
		{
			const FPCGPoint& Effector = InPoints[i];


			PrepareSinglePoint(i);

			// Flatten bounds

			const FTransform& Transform = Effector.Transform;
			Transforms[i] = Transform;
			Potencies[i] = PotencyValue->Read(i);
			Weights[i] = WeightValue->Read(i);

			FBox ScaledBounds = PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::ScaledBounds>(Effector);
			FBox WorldBounds = ScaledBounds.TransformBy(Transform);
			FVector Extents = ScaledBounds.GetExtent();

			Radiuses[i] = Extents.SquaredLength();

			Octree->AddElement(PCGEx::FIndexedItem(i, FBoxSphereBounds(PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::Bounds>(Effector).TransformBy(Transform))));

			//Effector.BoundsMin = Extents * -1;
			//Effector.BoundsMax = Extents;

			// Flatten original bounds
			//Effector.Transform.SetLocation(WorldBounds.GetCenter());
			//Effector.Transform.SetScale3D(FVector::OneVector);

			//Effector.BoundsMin = ScaledBounds.Min;
			//Effector.BoundsMax = ScaledBounds.Max;

			//Effector.Color = FVector4(Extents.X, Extents.Y, Extents.Z, Extents.SquaredLength()); // Cache Scaled Extents + Squared radius into $Color
			//Effector.Density = PotencyValue->Read(i);                                                     // Pack Weight to $Density
			//Effector.Steepness = WeightValue->Read(i);                                                  // Pack Potency to $Steepness
		}

		return true;
	}

	void FEffectorsArray::PrepareSinglePoint(const int32 Index)
	{
	}

	FTensorSample::FTensorSample(const FVector& InDirectionAndSize, const FQuat& InRotation, const int32 InEffectors, const double InWeight)
		: DirectionAndSize(InDirectionAndSize), Rotation(InRotation), Effectors(InEffectors), Weight(InWeight)
	{
	}

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
	FEffectorSample::FEffectorSample(const FVector& InDirection, const double InPotency, const double InWeight)
		: Direction(InDirection), Potency(InPotency), Weight(InWeight)
	{
	}

	FEffectorSample& FEffectorSamples::Emplace_GetRef(const FVector& InDirection, const double InPotency, const double InWeight)
	{
		TotalPotency += InPotency;
		TensorSample.Weight += InWeight;
		return Samples.Emplace_GetRef(InDirection, InPotency, InWeight);
	}
}
