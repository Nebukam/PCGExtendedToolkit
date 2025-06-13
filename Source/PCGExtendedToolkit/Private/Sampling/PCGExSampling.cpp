// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampling.h"

bool FPCGExApplySamplingDetails::WantsApply() const
{
	return AppliedComponents > 0;
}

void FPCGExApplySamplingDetails::Init()
{
#define PCGEX_REGISTER_FLAG(_COMPONENT, _ARRAY) \
if ((_COMPONENT & static_cast<uint8>(EPCGExApplySampledComponentFlags::X)) != 0){ _ARRAY.Add(0); AppliedComponents++; } \
if ((_COMPONENT & static_cast<uint8>(EPCGExApplySampledComponentFlags::Y)) != 0){ _ARRAY.Add(1); AppliedComponents++; } \
if ((_COMPONENT & static_cast<uint8>(EPCGExApplySampledComponentFlags::Z)) != 0){ _ARRAY.Add(2); AppliedComponents++; }

	if (bApplyTransform)
	{
		PCGEX_REGISTER_FLAG(TransformPosition, TrPosComponents)
		PCGEX_REGISTER_FLAG(TransformRotation, TrRotComponents)
		PCGEX_REGISTER_FLAG(TransformScale, TrScaComponents)
	}

	if (bApplyLookAt)
	{
		//PCGEX_REGISTER_FLAG(LookAtPosition, LkPosComponents)
		PCGEX_REGISTER_FLAG(LookAtRotation, LkRotComponents)
		//PCGEX_REGISTER_FLAG(LookAtScale, LkScaComponents)
	}

#undef PCGEX_REGISTER_FLAG
}

void FPCGExApplySamplingDetails::Apply(PCGExData::FMutablePoint& InPoint, const FTransform& InTransform, const FTransform& InLookAt)
{
	FTransform& T = InPoint.GetMutableTransform();

	FVector OutRotation = T.GetRotation().Euler();
	FVector OutPosition = T.GetLocation();
	FVector OutScale = T.GetScale3D();

	if (bApplyTransform)
	{
		const FVector InTrRot = InTransform.GetRotation().Euler();
		for (const int32 C : TrRotComponents) { OutRotation[C] = InTrRot[C]; }

		FVector InTrPos = InTransform.GetLocation();
		for (const int32 C : TrPosComponents) { OutPosition[C] = InTrPos[C]; }

		FVector InTrSca = InTransform.GetScale3D();
		for (const int32 C : TrScaComponents) { OutScale[C] = InTrSca[C]; }
	}

	if (bApplyLookAt)
	{
		const FVector InLkRot = InLookAt.GetRotation().Euler();
		for (const int32 C : LkRotComponents) { OutRotation[C] = InLkRot[C]; }

		//FVector InLkPos = InLookAt.GetLocation();
		//for (const int32 C : LkPosComponents) { OutPosition[C] = InLkPos[C]; }

		//FVector InLkSca = InLookAt.GetScale3D();
		//for (const int32 C : LkScaComponents) { OutScale[C] = InLkSca[C]; }
	}

	T = FTransform(FQuat::MakeFromEuler(OutRotation), OutPosition, OutScale);
}

namespace PCGExSampling
{
	double GetAngle(const EPCGExAngleRange Mode, const FVector& A, const FVector& B)
	{
		double OutAngle = 0;

		const FVector N1 = A.GetSafeNormal();
		const FVector N2 = B.GetSafeNormal();

		const double MainDot = N1.Dot(N2);
		const FVector C = FVector::CrossProduct(N1, N2);

		switch (Mode)
		{
		case EPCGExAngleRange::URadians: // 0 .. 3.14
			OutAngle = FMath::Acos(MainDot);
			break;
		case EPCGExAngleRange::PIRadians: // -3.14 .. 3.14
			OutAngle = FMath::Acos(MainDot) * FMath::Sign(MainDot);
			break;
		case EPCGExAngleRange::TAURadians: // 0 .. 6.28
			if (C.Z < 0) { OutAngle = TWO_PI - FMath::Atan2(C.Size(), MainDot); }
			else { OutAngle = FMath::Atan2(C.Size(), MainDot); }
			break;
		case EPCGExAngleRange::UDegrees: // 0 .. 180
			OutAngle = FMath::RadiansToDegrees(FMath::Acos(MainDot));
			break;
		case EPCGExAngleRange::PIDegrees: // -180 .. 180
			OutAngle = FMath::RadiansToDegrees(FMath::Acos(MainDot)) * FMath::Sign(MainDot);
			break;
		case EPCGExAngleRange::TAUDegrees: // 0 .. 360
			if (C.Z < 0) { OutAngle = 360 - FMath::RadiansToDegrees(FMath::Atan2(C.Size(), MainDot)); }
			else { OutAngle = FMath::RadiansToDegrees(FMath::Atan2(C.Size(), MainDot)); }
			break;
		case EPCGExAngleRange::NormalizedHalf: // 0 .. 180 -> 0 .. 1
			OutAngle = FMath::RadiansToDegrees(FMath::Acos(MainDot)) / 180;
			break;
		case EPCGExAngleRange::Normalized: // 0 .. 360 -> 0 .. 1
			if (C.Z < 0) { OutAngle = 360 - FMath::RadiansToDegrees(FMath::Atan2(C.Size(), MainDot)); }
			else { OutAngle = FMath::RadiansToDegrees(FMath::Atan2(C.Size(), MainDot)); }
			OutAngle /= 360;
			break;
		case EPCGExAngleRange::InvertedNormalizedHalf: // 0 .. 180 -> 1 .. 0
			OutAngle = 1 - (FMath::RadiansToDegrees(FMath::Acos(MainDot)) / 180);
			break;
		case EPCGExAngleRange::InvertedNormalized: // 0 .. 360 -> 1 .. 0
			if (C.Z < 0) { OutAngle = 360 - FMath::RadiansToDegrees(FMath::Atan2(C.Size(), MainDot)); }
			else { OutAngle = FMath::RadiansToDegrees(FMath::Atan2(C.Size(), MainDot)); }
			OutAngle /= 360;
			OutAngle = 1 - OutAngle;
			break;
		default: ;
		}

		return OutAngle;
	}

	bool GetIncludedActors(
		const FPCGContext* InContext,
		const TSharedRef<PCGExData::FFacade>& InFacade,
		const FName ActorReferenceName, TMap<AActor*, int32>& OutActorSet)
	{
		FPCGAttributePropertyInputSelector Selector = FPCGAttributePropertyInputSelector();
		Selector.SetAttributeName(ActorReferenceName);

		const TUniquePtr<PCGEx::TAttributeBroadcaster<FSoftObjectPath>> ActorReferences = MakeUnique<PCGEx::TAttributeBroadcaster<FSoftObjectPath>>();
		if (!ActorReferences->Prepare(Selector, InFacade->Source))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Actor reference attribute does not exist."));
			return false;
		}

		ActorReferences->Grab();

		for (int i = 0; i < ActorReferences->Values.Num(); i++)
		{
			const FSoftObjectPath& Path = ActorReferences->Values[i];
			if (!Path.IsValid()) { continue; }
			if (AActor* TargetActor = Cast<AActor>(Path.ResolveObject())) { OutActorSet.FindOrAdd(TargetActor, i); }
		}

		return true;
	}


	int32 FSampingUnionData::ComputeWeights(
		const TArray<const UPCGBasePointData*>& Sources, const TSharedPtr<PCGEx::FIndexLookup>& IdxLookup,
		const PCGExData::FConstPoint& Target, const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails,
		TArray<PCGExData::FWeightedPoint>& OutWeightedPoints) const
	{
		const int32 NumElements = Elements.Num();
		OutWeightedPoints.Reset(NumElements);

		double TotalWeight = 0;
		int32 Index = 0;

		if (WeightRange == -1)
		{
			double InternalRange = 0;
			for (const TPair<PCGExData::FElement, double>& W : Weights) { InternalRange = FMath::Max(InternalRange, W.Value); }

			// Remap weight to available max
			for (const PCGExData::FElement& Element : Elements)
			{
				const int32 IOIdx = IdxLookup->Get(Element.IO);
				if (IOIdx == -1) { continue; }

				const double Weight = 1 - (Weights[Element] / InternalRange);
				OutWeightedPoints.Emplace(Element.Index, Weight, IOIdx);
				TotalWeight += Weight;
				Index++;
			}
		}
		else
		{
			// Remap weight to specified max
			for (const PCGExData::FElement& Element : Elements)
			{
				const int32 IOIdx = IdxLookup->Get(Element.IO);
				if (IOIdx == -1) { continue; }

				const double Weight = 1 - (Weights[Element] / WeightRange);
				OutWeightedPoints.Emplace(Element.Index, Weight, IOIdx);
				TotalWeight += Weight;
				Index++;
			}
		}


		if (Index == 0) { return 0; }
		if (TotalWeight == 0)
		{
			const double FixedWeight = 1 / static_cast<double>(Index);
			for (PCGExData::FWeightedPoint& P : OutWeightedPoints) { P.Weight = FixedWeight; }
			return Index;
		}

		// Normalize weights
		//for (PCGExData::FWeightedPoint& P : OutWeightedPoints) { P.Weight /= TotalWeight; }
		return Index;
	}

	void FSampingUnionData::AddWeighted_Unsafe(const PCGExData::FElement& Element, const double InWeight)
	{
		Add_Unsafe(Element);
		Weights.Add(Element, InWeight);
	}

	void FSampingUnionData::AddWeighted(const PCGExData::FElement& Element, const double InWeight)
	{
		FWriteScopeLock WriteScopeLock(UnionLock);
		Add_Unsafe(Element);
		Weights.Add(Element, InWeight);
	}

	double FSampingUnionData::GetWeightAverage() const
	{
		if (Weights.Num() == 0) { return 0; }

		double Average = 0;
		for (const TPair<PCGExData::FElement, double>& Pair : Weights) { Average += Pair.Value; }
		return Average / Weights.Num();
	}

	double FSampingUnionData::GetSqrtWeightAverage() const
	{
		if (Weights.Num() == 0) { return 0; }

		double Average = 0;
		for (const TPair<PCGExData::FElement, double>& Pair : Weights) { Average += FMath::Sqrt(Pair.Value); }
		return Average / Weights.Num();
	}
}
