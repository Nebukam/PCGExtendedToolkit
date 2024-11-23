// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestSurface.h"


#if PCGEX_ENGINE_VERSION > 503
#include "Engine/OverlapResult.h"
#endif

#define LOCTEXT_NAMESPACE "PCGExSampleNearestSurfaceElement"
#define PCGEX_NAMESPACE SampleNearestSurface

TArray<FPCGPinProperties> UPCGExSampleNearestSurfaceSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (SurfaceSource == EPCGExSurfaceSource::ActorReferences) { PCGEX_PIN_POINT(PCGExSampling::SourceActorReferencesLabel, "Points with actor reference paths.", Required, {}) }
	return PinProperties;
}

PCGExData::EIOInit UPCGExSampleNearestSurfaceSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

int32 UPCGExSampleNearestSurfaceSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_L; }

PCGEX_INITIALIZE_ELEMENT(SampleNearestSurface)

bool FPCGExSampleNearestSurfaceElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestSurface)

	PCGEX_FOREACH_FIELD_NEARESTSURFACE(PCGEX_OUTPUT_VALIDATE_NAME)

	Context->bUseInclude = Settings->SurfaceSource == EPCGExSurfaceSource::ActorReferences;
	if (Context->bUseInclude)
	{
		PCGEX_VALIDATE_NAME(Settings->ActorReference)

		Context->ActorReferenceDataFacade = PCGExData::TryGetSingleFacade(Context, PCGExSampling::SourceActorReferencesLabel, true);
		if (!Context->ActorReferenceDataFacade) { return false; }

		if (!PCGExSampling::GetIncludedActors(
			Context, Context->ActorReferenceDataFacade.ToSharedRef(),
			Settings->ActorReference, Context->IncludedActors))
		{
			return false;
		}

		TSet<UPrimitiveComponent*> IncludedPrimitiveSet;
		for (const TPair<AActor*, int32> Pair : Context->IncludedActors)
		{
			TArray<UPrimitiveComponent*> FoundPrimitives;
			Pair.Key->GetComponents(UPrimitiveComponent::StaticClass(), FoundPrimitives);
			for (UPrimitiveComponent* Primitive : FoundPrimitives) { IncludedPrimitiveSet.Add(Primitive); }
		}

		if (IncludedPrimitiveSet.IsEmpty())
		{
			// TODO : Throw
			return false;
		}

		Context->IncludedPrimitives.Reserve(IncludedPrimitiveSet.Num());
		Context->IncludedPrimitives.Append(IncludedPrimitiveSet.Array());
	}

	Context->CollisionSettings = Settings->CollisionSettings;
	Context->CollisionSettings.Init(Context);

	return true;
}

bool FPCGExSampleNearestSurfaceElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestSurfaceElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestSurface)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSampleNearestSurface::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExSampleNearestSurface::FProcessor>>& NewBatch)
			{
				if (Settings->bPruneFailedSamples) { NewBatch->bRequiresWriteStep = true; }
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to sample."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExSampleNearestSurface
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleNearestSurface::Process);

		SurfacesForward = Context->bUseInclude ? Settings->AttributesForwarding.TryGetHandler(Context->ActorReferenceDataFacade, PointDataFacade) : nullptr;

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		SampleState.SetNumUninitialized(PointDataFacade->GetNum());

		{
			const TSharedRef<PCGExData::FFacade>& OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_NEARESTSURFACE(PCGEX_OUTPUT_INIT)
		}

		if (Settings->bUseLocalMaxDistance)
		{
			MaxDistanceGetter = PointDataFacade->GetScopedBroadcaster<double>(Settings->LocalMaxDistance);
			if (!MaxDistanceGetter)
			{
				PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("LocalMaxDistance missing"));
				return false;
			}
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
		FilterScope(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		const double MaxDistance = MaxDistanceGetter ? MaxDistanceGetter->Read(Index) : Settings->MaxDistance;

		auto SamplingFailed = [&]()
		{
			SampleState[Index] = false;

			const FVector Direction = FVector::UpVector;
			PCGEX_OUTPUT_VALUE(Location, Index, Point.Transform.GetLocation())
			PCGEX_OUTPUT_VALUE(Normal, Index, Direction*-1) // TODO: expose "precise normal" in which case we line trace to location
			PCGEX_OUTPUT_VALUE(LookAt, Index, Direction)
			PCGEX_OUTPUT_VALUE(Distance, Index, MaxDistance)
			//PCGEX_OUTPUT_VALUE(IsInside, Index, false)
			//PCGEX_OUTPUT_VALUE(Success, Index, false)
			//PCGEX_OUTPUT_VALUE(ActorReference, Index, TEXT(""))
			//PCGEX_OUTPUT_VALUE(PhysMat, Index, TEXT(""))
		};

		if (!PointFilterCache[Index])
		{
			if (Settings->bProcessFilteredOutAsFails) { SamplingFailed(); }
			return;
		}

		const FVector Origin = PointDataFacade->Source->GetInPoint(Index).Transform.GetLocation();

		FCollisionQueryParams CollisionParams;
		Context->CollisionSettings.Update(CollisionParams);

		const FCollisionShape CollisionShape = FCollisionShape::MakeSphere(MaxDistance);

		FVector HitLocation;
		const int32* HitIndex = nullptr;
		bool bSuccess = false;
		TArray<FOverlapResult> OutOverlaps;

		auto ProcessOverlapResults = [&]()
		{
			float MinDist = MAX_FLT;
			UPrimitiveComponent* HitComp = nullptr;
			for (const FOverlapResult& Overlap : OutOverlaps)
			{
				//if (!Overlap.bBlockingHit) { continue; }
				if (Context->bUseInclude && !Context->IncludedActors.Contains(Overlap.GetActor())) { continue; }

				FVector OutClosestLocation;
				const float Distance = Overlap.Component->GetClosestPointOnCollision(Origin, OutClosestLocation);

				if (Distance < 0) { continue; }

				if (Distance < MinDist)
				{
					HitIndex = Context->IncludedActors.Find(Overlap.GetActor());
					MinDist = Distance;
					HitLocation = OutClosestLocation;
					bSuccess = true;
					HitComp = Overlap.Component.Get();
				}
			}

			if (bSuccess)
			{
				const FVector Direction = (HitLocation - Origin).GetSafeNormal();

				PCGEX_OUTPUT_VALUE(LookAt, Index, Direction)

				FVector HitNormal = Direction * -1;
				bool bIsInside = MinDist == 0;

				if (SurfacesForward && HitIndex) { SurfacesForward->Forward(*HitIndex, Index); }

				if (HitComp)
				{
					if (Context->CollisionSettings.bTraceComplex)
					{
						FCollisionQueryParams PreciseCollisionParams;
						PreciseCollisionParams.bTraceComplex = true;
						PreciseCollisionParams.bReturnPhysicalMaterial = PhysMatWriter ? true : false;

						FHitResult HitResult;
						if (HitComp->LineTraceComponent(HitResult, HitLocation - Direction, HitLocation + Direction, PreciseCollisionParams))
						{
							HitNormal = HitResult.ImpactNormal;
							HitLocation = HitResult.Location;
							bIsInside = IsInsideWriter ? FVector::DotProduct(Direction, HitResult.ImpactNormal) > 0 : false;

#if PCGEX_ENGINE_VERSION <= 503
							if (const AActor* HitActor = HitResult.GetActor()) { PCGEX_OUTPUT_VALUE(ActorReference, Index, HitActor->GetPathName()) }
							if (const UPhysicalMaterial* PhysMat = HitResult.PhysMaterial.Get()) { PCGEX_OUTPUT_VALUE(PhysMat, Index, PhysMat->GetPathName()) }
#else
							if (const AActor* HitActor = HitResult.GetActor()) { PCGEX_OUTPUT_VALUE(ActorReference, Index, FSoftObjectPath(HitActor->GetPathName())) }
							if (const UPhysicalMaterial* PhysMat = HitResult.PhysMaterial.Get()) { PCGEX_OUTPUT_VALUE(PhysMat, Index, FSoftObjectPath(PhysMat->GetPathName())) }
#endif
						}
					}
					else
					{
						UPhysicalMaterial* PhysMat = HitComp->GetBodyInstance()->GetSimplePhysicalMaterial();

#if PCGEX_ENGINE_VERSION <= 503
						PCGEX_OUTPUT_VALUE(ActorReference, Index, HitComp->GetOwner()->GetPathName())
						if (PhysMat) { PCGEX_OUTPUT_VALUE(PhysMat, Index, PhysMat->GetPathName()) }
#else
						PCGEX_OUTPUT_VALUE(ActorReference, Index, FSoftObjectPath(HitComp->GetOwner()->GetPathName()))
						if (PhysMat) { PCGEX_OUTPUT_VALUE(PhysMat, Index, FSoftObjectPath(PhysMat->GetPathName())) }
#endif
					}
				}

				PCGEX_OUTPUT_VALUE(Location, Index, HitLocation)
				PCGEX_OUTPUT_VALUE(Normal, Index, HitNormal)
				PCGEX_OUTPUT_VALUE(IsInside, Index, bIsInside)
				PCGEX_OUTPUT_VALUE(Distance, Index, MinDist)
				PCGEX_OUTPUT_VALUE(Success, Index, true)
				SampleState[Index] = true;

				FPlatformAtomics::InterlockedExchange(&bAnySuccess, 1);
			}
			else
			{
				SamplingFailed();
			}
		};


		if (Settings->SurfaceSource == EPCGExSurfaceSource::ActorReferences)
		{
			for (const UPrimitiveComponent* Primitive : Context->IncludedPrimitives)
			{
				if (TArray<FOverlapResult> TempOverlaps;
					Primitive->OverlapComponentWithResult(Origin, FQuat::Identity, CollisionShape, TempOverlaps))
				{
					OutOverlaps.Append(TempOverlaps);
				}
			}
			if (OutOverlaps.IsEmpty()) { SamplingFailed(); }
			else { ProcessOverlapResults(); }
		}
		else
		{
			const UWorld* World = Context->SourceComponent->GetWorld();

			switch (Context->CollisionSettings.CollisionType)
			{
			case EPCGExCollisionFilterType::Channel:
				if (World->OverlapMultiByChannel(OutOverlaps, Origin, FQuat::Identity, Context->CollisionSettings.CollisionChannel, CollisionShape, CollisionParams))
				{
					ProcessOverlapResults();
				}
				else { SamplingFailed(); }
				break;
			case EPCGExCollisionFilterType::ObjectType:
				if (World->OverlapMultiByObjectType(OutOverlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(Context->CollisionSettings.CollisionObjectType), CollisionShape, CollisionParams))
				{
					ProcessOverlapResults();
				}
				else { SamplingFailed(); }
				break;
			case EPCGExCollisionFilterType::Profile:
				if (World->OverlapMultiByProfile(OutOverlaps, Origin, FQuat::Identity, Context->CollisionSettings.CollisionProfileName, CollisionShape, CollisionParams))
				{
					ProcessOverlapResults();
				}
				else { SamplingFailed(); }
				break;
			default:
				SamplingFailed();
				break;
			}
		}
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);

		if (Settings->bTagIfHasSuccesses && bAnySuccess) { PointDataFacade->Source->Tags->Add(Settings->HasSuccessesTag); }
		if (Settings->bTagIfHasNoSuccesses && !bAnySuccess) { PointDataFacade->Source->Tags->Add(Settings->HasNoSuccessesTag); }
	}

	void FProcessor::Write()
	{
		PCGExSampling::PruneFailedSamples(PointDataFacade->GetMutablePoints(), SampleState);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
