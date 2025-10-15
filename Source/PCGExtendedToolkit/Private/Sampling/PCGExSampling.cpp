// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampling.h"

#include "GameFramework/Actor.h"
#include "PCGExPointsProcessor.h"
#include "Data/Matching/PCGExMatchRuleFactoryProvider.h"
#include "Details/PCGExDetailsDistances.h"

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

		if (WeightRange == -2)
		{
			// Don't remap weight
			for (const PCGExData::FElement& Element : Elements)
			{
				const int32 IOIdx = IdxLookup->Get(Element.IO);
				if (IOIdx == -1) { continue; }

				const double Weight = Weights[Element];
				OutWeightedPoints.Emplace(Element.Index, Weight, IOIdx);
				TotalWeight += Weight;
				Index++;
			}
		}
		else if (WeightRange == -1)
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


	int32 FTargetsHandler::Init(FPCGExContext* InContext, const FName InPinLabel, FInitData&& InitFn)
	{
		FBox OctreeBounds = FBox(ForceInit);
		
		TSharedPtr<PCGExData::FPointIOCollection> Targets = MakeShared<PCGExData::FPointIOCollection>(
			InContext, InPinLabel, PCGExData::EIOInit::NoInit, true);

		if (Targets->IsEmpty())
		{
			PCGEX_LOG_MISSING_INPUT(InContext, FTEXT("No targets (empty datasets)"))
			return 0;
		}

		TargetFacades.Reserve(Targets->Pairs.Num());
		TargetOctrees.Reserve(Targets->Pairs.Num());

		TArray<FBox> Bounds;
		Bounds.Reserve(Targets->Pairs.Num());

		int32 Idx = 0;
		for (const TSharedPtr<PCGExData::FPointIO>& IO : Targets->Pairs)
		{
			const FBox DataBounds = InitFn(IO, Idx);
			if (!DataBounds.IsValid) { continue; }

			TSharedPtr<PCGExData::FFacade> TargetFacade = MakeShared<PCGExData::FFacade>(IO.ToSharedRef());

			TargetFacade->Idx = Idx;
			TargetFacades.Add(TargetFacade.ToSharedRef());
			TargetOctrees.Add(&TargetFacade->GetIn()->GetPointOctree());

			MaxNumTargets = FMath::Max(MaxNumTargets, TargetFacade->GetNum());

			Bounds.Emplace(DataBounds);
			OctreeBounds += DataBounds;

			Idx++;
		}

		if (TargetFacades.IsEmpty()) { return 0; }

		TargetsOctree = MakeShared<PCGExOctree::FItemOctree>(OctreeBounds.GetCenter(), OctreeBounds.GetExtent().Length());
		for (int i = 0; i < TargetFacades.Num(); ++i) { TargetsOctree->AddElement(PCGExOctree::FItem(i, Bounds[i])); }

		TargetsPreloader = MakeShared<PCGExData::FMultiFacadePreloader>(TargetFacades);

		return TargetFacades.Num();
	}

	int32 FTargetsHandler::Init(FPCGExContext* InContext, const FName InPinLabel)
	{
		return Init(InContext, InPinLabel, [](const TSharedPtr<PCGExData::FPointIO>& IO, const int32 Idx) { return IO->GetIn()->GetBounds(); });
	}

	void FTargetsHandler::SetDistances(const FPCGExDistanceDetails& InDetails)
	{
		Distances = InDetails.MakeDistances();
	}

	void FTargetsHandler::SetDistances(const EPCGExDistance Source, const EPCGExDistance Target, const bool bOverlapIsZero)
	{
		Distances = PCGExDetails::MakeDistances(Source, Target, bOverlapIsZero);
	}

	void FTargetsHandler::SetMatchingDetails(FPCGExContext* InContext, const FPCGExMatchingDetails* InDetails)
	{
		DataMatcher = MakeShared<PCGExMatching::FDataMatcher>();
		DataMatcher->SetDetails(InDetails);
		if (!DataMatcher->Init(InContext, TargetFacades, false)) { DataMatcher.Reset(); }
	}

	bool FTargetsHandler::PopulateIgnoreList(const TSharedPtr<PCGExData::FPointIO>& InDataCandidate, PCGExMatching::FMatchingScope& InMatchingScope, TSet<const UPCGData*>& OutIgnoreList) const
	{
		if (DataMatcher) { return DataMatcher->PopulateIgnoreList(InDataCandidate, InMatchingScope, OutIgnoreList); }
		return true;
	}

	bool FTargetsHandler::HandleUnmatchedOutput(const TSharedPtr<PCGExData::FFacade>& InFacade, const bool bForward) const
	{
		if (DataMatcher) { return DataMatcher->HandleUnmatchedOutput(InFacade, bForward); }
		return false;
	}


	void FTargetsHandler::ForEachPreloader(PCGExData::FMultiFacadePreloader::FPreloaderItCallback&& It) const
	{
		TargetsPreloader->ForEach(MoveTemp(It));
	}

	void FTargetsHandler::ForEachTarget(FFacadeRefIterator&& It, const TSet<const UPCGData*>* Exclude) const
	{
		for (int i = 0; i < TargetFacades.Num(); i++)
		{
			const TSharedRef<PCGExData::FFacade>& Target = TargetFacades[i];
			if (Exclude && Exclude->Contains(Target->GetIn())) { continue; }
			It(Target, i);
		}
	}

	bool FTargetsHandler::ForEachTarget(FFacadeRefIteratorWithBreak&& It, const TSet<const UPCGData*>* Exclude) const
	{
		bool bBreak = false;
		for (int i = 0; i < TargetFacades.Num(); i++)
		{
			const TSharedRef<PCGExData::FFacade>& Target = TargetFacades[i];
			if (Exclude && Exclude->Contains(Target->GetIn())) { continue; }
			It(Target, i, bBreak);
			if (bBreak) { return true; }
		}

		return bBreak;
	}

	void FTargetsHandler::ForEachTargetPoint(FPointIterator&& It, const TSet<const UPCGData*>* Exclude) const
	{
		for (int i = 0; i < TargetFacades.Num(); i++)
		{
			if (Exclude && Exclude->Contains(TargetFacades[i]->GetIn())) { return; }
			const int32 NumPoints = TargetFacades[i]->GetNum();
			for (int j = 0; j < NumPoints; j++) { It(PCGExData::FPoint(j, i)); }
		}
	}

	void FTargetsHandler::ForEachTargetPoint(FPointIteratorWithData&& It, const TSet<const UPCGData*>* Exclude) const
	{
		for (int i = 0; i < TargetFacades.Num(); i++)
		{
			const TSharedRef<PCGExData::FFacade>& Target = TargetFacades[i];
			if (Exclude && Exclude->Contains(Target->GetIn())) { return; }
			const int32 NumPoints = TargetFacades[i]->GetNum();
			for (int j = 0; j < NumPoints; j++)
			{
				PCGExData::FConstPoint Point = Target->GetInPoint(j);
				Point.IO = i;
				It(Point);
			}
		}
	}

	void FTargetsHandler::FindTargetsWithBoundsTest(const FBoxCenterAndExtent& QueryBounds, FTargetQuery&& Func, const TSet<const UPCGData*>* Exclude) const
	{
		TargetsOctree->FindElementsWithBoundsTest(
			QueryBounds, [&](const PCGExOctree::FItem& Item)
			{
				if (Exclude && Exclude->Contains(TargetFacades[Item.Index]->GetIn())) { return; }
				Func(Item);
			});
	}

	void FTargetsHandler::FindElementsWithBoundsTest(const FBoxCenterAndExtent& QueryBounds, FTargetElementsQuery&& Func, const TSet<const UPCGData*>* Exclude) const
	{
		TargetsOctree->FindElementsWithBoundsTest(
			QueryBounds, [&](const PCGExOctree::FItem& Item)
			{
				if (Exclude && Exclude->Contains(TargetFacades[Item.Index]->GetIn())) { return; }

				TargetOctrees[Item.Index]->FindElementsWithBoundsTest(
					QueryBounds, [&](const PCGPointOctree::FPointRef& PointRef)
					{
						Func(PCGExData::FPoint(PointRef.Index, Item.Index));
					});
			});
	}

	void FTargetsHandler::FindElementsWithBoundsTest(const FBoxCenterAndExtent& QueryBounds, FOctreeQueryWithData&& Func, const TSet<const UPCGData*>* Exclude) const
	{
		TargetsOctree->FindElementsWithBoundsTest(
			QueryBounds, [&](const PCGExOctree::FItem& Item)
			{
				const TSharedRef<PCGExData::FFacade>& Target = TargetFacades[Item.Index];
				if (Exclude && Exclude->Contains(Target->GetIn())) { return; }

				TargetOctrees[Item.Index]->FindElementsWithBoundsTest(
					QueryBounds, [&](const PCGPointOctree::FPointRef& PointRef)
					{
						PCGExData::FConstPoint Point = Target->GetInPoint(PointRef.Index);
						Point.IO = Item.Index;
						Func(Point);
					});
			});
	}

	bool FTargetsHandler::FindClosestTarget(
		const PCGExData::FConstPoint& Probe,
		const FBoxCenterAndExtent& QueryBounds,
		PCGExData::FConstPoint& OutResult,
		double& OutDistSquared,
		const TSet<const UPCGData*>* Exclude) const
	{
		bool bFound = false;

		if (Distances->bOverlapIsZero)
		{
			TargetsOctree->FindElementsWithBoundsTest(
				QueryBounds, [&](const PCGExOctree::FItem& Item)
				{
					const TSharedRef<PCGExData::FFacade>& Target = TargetFacades[Item.Index];
					const bool bSelf = Target->GetIn() == Probe.Data;

					if (Exclude && Exclude->Contains(Target->GetIn())) { return; }

					TargetOctrees[Item.Index]->FindElementsWithBoundsTest(
						QueryBounds, [&](const PCGPointOctree::FPointRef& PointRef)
						{
							if (bSelf && PointRef.Index == Probe.Index) { return; }

							PCGExData::FConstPoint Point = Target->GetInPoint(PointRef.Index);
							bool bOverlap = false;
							double Dist = Distances->GetDistSquared(Probe, Point, bOverlap);
							if (bOverlap) { Dist = 0; }

							if (OutDistSquared > Dist)
							{
								OutResult = Point;
								OutResult.IO = Item.Index;

								OutDistSquared = Dist;
								bFound = true;
							}
						});
				});
		}
		else
		{
			TargetsOctree->FindElementsWithBoundsTest(
				QueryBounds, [&](const PCGExOctree::FItem& Item)
				{
					const TSharedRef<PCGExData::FFacade>& Target = TargetFacades[Item.Index];
					const bool bSelf = Target->GetIn() == Probe.Data;

					if (Exclude && Exclude->Contains(Target->GetIn())) { return; }

					TargetOctrees[Item.Index]->FindElementsWithBoundsTest(
						QueryBounds, [&](const PCGPointOctree::FPointRef& PointRef)
						{
							if (bSelf && PointRef.Index == Probe.Index) { return; }

							const PCGExData::FConstPoint Point = Target->GetInPoint(PointRef.Index);
							if (const double Dist = Distances->GetDistSquared(Probe, Point);
								OutDistSquared > Dist)
							{
								OutResult = Point;
								OutResult.IO = Item.Index;

								OutDistSquared = Dist;
								bFound = true;
							}
						});
				});
		}

		return bFound;
	}

	void FTargetsHandler::FindClosestTarget(
		const PCGExData::FConstPoint& Probe,
		PCGExData::FConstPoint& OutResult,
		double& OutDistSquared,
		const TSet<const UPCGData*>* Exclude) const
	{
		const FVector ProbeLocation = Probe.GetLocation();

		if (Distances->bOverlapIsZero)
		{
			TargetsOctree->FindNearbyElements(
				ProbeLocation, [&](const PCGExOctree::FItem& Item)
				{
					const TSharedRef<PCGExData::FFacade>& Target = TargetFacades[Item.Index];
					const bool bSelf = Target->GetIn() == Probe.Data;

					if (Exclude && Exclude->Contains(Target->GetIn())) { return; }

					TargetOctrees[Item.Index]->FindNearbyElements(
						ProbeLocation, [&](const PCGPointOctree::FPointRef& PointRef)
						{
							if (bSelf && PointRef.Index == Probe.Index) { return; }

							const PCGExData::FConstPoint Point = Target->GetInPoint(PointRef.Index);
							bool bOverlap = false;
							double Dist = Distances->GetDistSquared(Probe, Point, bOverlap);
							if (bOverlap) { Dist = 0; }

							if (OutDistSquared > Dist)
							{
								OutResult = Point;
								OutResult.IO = Item.Index;

								OutDistSquared = Dist;
							}
						});
				});
		}
		else
		{
			TargetsOctree->FindNearbyElements(
				ProbeLocation, [&](const PCGExOctree::FItem& Item)
				{
					const TSharedRef<PCGExData::FFacade>& Target = TargetFacades[Item.Index];
					const bool bSelf = Target->GetIn() == Probe.Data;

					if (Exclude && Exclude->Contains(Target->GetIn())) { return; }

					TargetOctrees[Item.Index]->FindNearbyElements(
						ProbeLocation, [&](const PCGPointOctree::FPointRef& PointRef)
						{
							if (bSelf && PointRef.Index == Probe.Index) { return; }

							const PCGExData::FConstPoint Point = Target->GetInPoint(PointRef.Index);
							if (const double Dist = Distances->GetDistSquared(Probe, Point);
								OutDistSquared > Dist)
							{
								OutResult = Point;
								OutResult.IO = Item.Index;

								OutDistSquared = Dist;
							}
						});
				});
		}
	}

	void FTargetsHandler::FindClosestTarget(
		const FVector& Probe,
		PCGExData::FConstPoint& OutResult,
		double& OutDistSquared,
		const TSet<const UPCGData*>* Exclude) const
	{
		TargetsOctree->FindNearbyElements(
			Probe, [&](const PCGExOctree::FItem& Item)
			{
				const TSharedRef<PCGExData::FFacade>& Target = TargetFacades[Item.Index];
				if (Exclude && Exclude->Contains(Target->GetIn())) { return; }

				TargetOctrees[Item.Index]->FindNearbyElements(
					Probe, [&](const PCGPointOctree::FPointRef& PointRef)
					{
						PCGExData::FConstPoint Point = Target->GetInPoint(PointRef.Index);

						if (const double Dist = FVector::DistSquared(Distances->GetTargetCenter(Point, Point.GetLocation(), Probe), Probe);
							OutDistSquared > Dist)
						{
							OutResult = Point;
							OutResult.IO = Item.Index;

							OutDistSquared = Dist;
						}
					});
			});
	}

	PCGExData::FConstPoint FTargetsHandler::GetPoint(const int32 IO, const int32 Index) const
	{
		return TargetFacades[IO]->GetInPoint(Index);
	}

	PCGExData::FConstPoint FTargetsHandler::GetPoint(const PCGExData::FPoint& Point) const
	{
		return TargetFacades[Point.IO]->GetInPoint(Point.Index);
	}

	double FTargetsHandler::GetDistSquared(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint) const
	{
		if (Distances->bOverlapIsZero)
		{
			bool bOverlap = false;
			double DistSquared = Distances->GetDistSquared(SourcePoint, TargetPoint, bOverlap);
			if (bOverlap) { DistSquared = 0; }
			return DistSquared;
		}
		return Distances->GetDistSquared(SourcePoint, TargetPoint);
	}

	FVector FTargetsHandler::GetSourceCenter(const PCGExData::FConstPoint& OriginPoint, const FVector& OriginLocation, const FVector& ToCenter) const
	{
		return Distances->GetSourceCenter(OriginPoint, OriginLocation, ToCenter);
	}

	void FTargetsHandler::StartLoading(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const TSharedPtr<PCGExMT::FAsyncMultiHandle>& InParentHandle) const
	{
		TargetsPreloader->StartLoading(AsyncManager, InParentHandle);
	}
}
