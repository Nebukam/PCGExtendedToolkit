// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestSurface.h"

#include "Data/PCGExPointFilter.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
#include "Engine/OverlapResult.h"
#endif

#define LOCTEXT_NAMESPACE "PCGExSampleNearestSurfaceElement"
#define PCGEX_NAMESPACE SampleNearestSurface

TArray<FPCGPinProperties> UPCGExSampleNearestSurfaceSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (SurfaceSource == EPCGExSurfaceSource::ActorReferences) { PCGEX_PIN_POINT(PCGExSampling::SourceActorReferencesLabel, "Points with actor reference paths.", Required, {}) }
	PCGEX_PIN_PARAMS(PCGEx::SourcePointFilters, "Filter which points will be processed.", Advanced, {})
	return PinProperties;
}

PCGExData::EInit UPCGExSampleNearestSurfaceSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

int32 UPCGExSampleNearestSurfaceSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_L; }

FName UPCGExSampleNearestSurfaceSettings::GetPointFilterLabel() const { return PCGExPointFilter::SourceFiltersLabel; }

PCGEX_INITIALIZE_ELEMENT(SampleNearestSurface)

FPCGExSampleNearestSurfaceContext::~FPCGExSampleNearestSurfaceContext()
{
	PCGEX_TERMINATE_ASYNC
	PCGEX_DELETE_FACADE_AND_SOURCE(ActorReferenceDataFacade)
}

bool FPCGExSampleNearestSurfaceElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestSurface)

	PCGEX_FOREACH_FIELD_NEARESTSURFACE(PCGEX_OUTPUT_VALIDATE_NAME)
	PCGEX_FOREACH_FIELD_NEARESTSURFACE_ACTOR(PCGEX_OUTPUT_VALIDATE_NAME)

	Context->bUseInclude = Settings->SurfaceSource == EPCGExSurfaceSource::ActorReferences;
	if (Context->bUseInclude)
	{
		PCGEX_VALIDATE_NAME(Settings->ActorReference)
		PCGExData::FPointIO* ActorRefIO = PCGExData::TryGetSingleInput(Context, PCGExSampling::SourceActorReferencesLabel, true);

		if (!ActorRefIO) { return false; }

		Context->ActorReferenceDataFacade = new PCGExData::FFacade(ActorRefIO);

		if (!PCGExSampling::GetIncludedActors(
			Context, Context->ActorReferenceDataFacade,
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

	return true;
}

bool FPCGExSampleNearestSurfaceElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestSurfaceElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestSurface)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (Settings->bIgnoreSelf) { Context->IgnoredActors.Add(Context->SourceComponent->GetOwner()); }

		if (Settings->bIgnoreActors)
		{
			const TFunction<bool(const AActor*)> BoundsCheck = [](const AActor*) -> bool { return true; };
			const TFunction<bool(const AActor*)> SelfIgnoreCheck = [](const AActor*) -> bool { return true; };
			const TArray<AActor*> IgnoredActors = PCGExActorSelector::FindActors(Settings->IgnoredActorSelector, Context->SourceComponent.Get(), BoundsCheck, SelfIgnoreCheck);
			Context->IgnoredActors.Append(IgnoredActors);
		}

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSampleNearestSurface::FProcessor>>(
			[&](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExSampleNearestSurface::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any points to sample."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExSampleNearestSurface
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(SurfacesForward)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleNearestSurface::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SampleNearestSurface)

		LocalTypedContext = TypedContext;
		LocalSettings = Settings;

		SurfacesForward = TypedContext->bUseInclude ? Settings->AttributesForwarding.TryGetHandler(TypedContext->ActorReferenceDataFacade, PointDataFacade) : nullptr;

		// Must be set before process for filters
		PointDataFacade->bSupportsDynamic = true;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		{
			PCGExData::FFacade* OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_NEARESTSURFACE(PCGEX_OUTPUT_INIT)
			PCGEX_FOREACH_FIELD_NEARESTSURFACE_ACTOR(PCGEX_OUTPUT_INIT_DEFAULT)
		}

		if (Settings->bUseLocalMaxDistance)
		{
			MaxDistanceGetter = PointDataFacade->GetScopedBroadcaster<double>(Settings->LocalMaxDistance);
			if (!MaxDistanceGetter)
			{
				PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("LocalMaxDistance missing"));
				return false;
			}
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		const double MaxDistance = MaxDistanceGetter ? MaxDistanceGetter->Values[Index] : LocalSettings->MaxDistance;

		auto SamplingFailed = [&]()
		{
			const FVector Direction = FVector::UpVector;
			PCGEX_OUTPUT_VALUE(Location, Index, Point.Transform.GetLocation())
			PCGEX_OUTPUT_VALUE(Normal, Index, Direction*-1) // TODO: expose "precise normal" in which case we line trace to location
			PCGEX_OUTPUT_VALUE(LookAt, Index, Direction)
			PCGEX_OUTPUT_VALUE(Distance, Index, MaxDistance)
			PCGEX_OUTPUT_VALUE(IsInside, Index, false)
			PCGEX_OUTPUT_VALUE(Success, Index, false)

			PCGEX_OUTPUT_VALUE(ActorReference, Index, TEXT(""))
			PCGEX_OUTPUT_VALUE(PhysMat, Index, TEXT(""))
		};

		if (PrimaryFilters && !PrimaryFilters->Test(Index))
		{
			SamplingFailed();
			return;
		}

		const FVector Origin = PointIO->GetInPoint(Index).Transform.GetLocation();

		FCollisionQueryParams CollisionParams;
		CollisionParams.bTraceComplex = LocalSettings->bTraceComplex;
		CollisionParams.AddIgnoredActors(LocalTypedContext->IgnoredActors);

		const FCollisionShape CollisionShape = FCollisionShape::MakeSphere(MaxDistance);

		FVector HitLocation;
		int32* HitIndex = nullptr;
		bool bSuccess = false;
		TArray<FOverlapResult> OutOverlaps;

		auto ProcessOverlapResults = [&]()
		{
			float MinDist = MAX_FLT;
			UPrimitiveComponent* HitComp = nullptr;
			for (const FOverlapResult& Overlap : OutOverlaps)
			{
				//if (!Overlap.bBlockingHit) { continue; }
				if (LocalTypedContext->bUseInclude && !LocalTypedContext->IncludedActors.Contains(Overlap.GetActor())) { continue; }

				FVector OutClosestLocation;
				const float Distance = Overlap.Component->GetClosestPointOnCollision(Origin, OutClosestLocation);

				if (Distance < 0) { continue; }

				if (Distance < MinDist)
				{
					HitIndex = LocalTypedContext->IncludedActors.Find(Overlap.GetActor());
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
					if (LocalSettings->bTraceComplex)
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

							if (const AActor* HitActor = HitResult.GetActor()) { PCGEX_OUTPUT_VALUE(ActorReference, Index, HitActor->GetPathName()) }
							else { PCGEX_OUTPUT_VALUE(ActorReference, Index, TEXT("")) }

							if (const UPhysicalMaterial* PhysMat = HitResult.PhysMaterial.Get()) { PCGEX_OUTPUT_VALUE(PhysMat, Index, PhysMat->GetPathName()) }
							else { PCGEX_OUTPUT_VALUE(PhysMat, Index, TEXT("")) }
						}
					}
					else
					{
						PCGEX_OUTPUT_VALUE(ActorReference, Index, HitComp->GetOwner()->GetPathName())
						UPhysicalMaterial* PhysMat = HitComp->GetBodyInstance()->GetSimplePhysicalMaterial();

						if (PhysMat) { PCGEX_OUTPUT_VALUE(PhysMat, Index, PhysMat->GetPathName()) }
						else { PCGEX_OUTPUT_VALUE(PhysMat, Index, TEXT("")) }
					}
				}
				else
				{
					PCGEX_OUTPUT_VALUE(ActorReference, Index, TEXT(""))
					PCGEX_OUTPUT_VALUE(PhysMat, Index, TEXT(""))
				}

				PCGEX_OUTPUT_VALUE(Location, Index, HitLocation)
				PCGEX_OUTPUT_VALUE(Normal, Index, HitNormal)
				PCGEX_OUTPUT_VALUE(IsInside, Index, bIsInside)
				PCGEX_OUTPUT_VALUE(Distance, Index, MinDist)
				PCGEX_OUTPUT_VALUE(Success, Index, true)
			}
			else
			{
				SamplingFailed();
			}
		};


		if (LocalSettings->SurfaceSource == EPCGExSurfaceSource::ActorReferences)
		{
			for (const UPrimitiveComponent* Primitive : LocalTypedContext->IncludedPrimitives)
			{
				TArray<FOverlapResult> TempOverlaps;
				if (Primitive->OverlapComponentWithResult(Origin, FQuat::Identity, CollisionShape, TempOverlaps))
				{
					OutOverlaps.Append(TempOverlaps);
				}
			}
			if (OutOverlaps.IsEmpty()) { SamplingFailed(); }
			else { ProcessOverlapResults(); }
		}
		else
		{
			switch (LocalSettings->CollisionType)
			{
			case EPCGExCollisionFilterType::Channel:
				if (LocalTypedContext->World->OverlapMultiByChannel(OutOverlaps, Origin, FQuat::Identity, LocalSettings->CollisionChannel, CollisionShape, CollisionParams))
				{
					ProcessOverlapResults();
				}
				else { SamplingFailed(); }
				break;
			case EPCGExCollisionFilterType::ObjectType:
				if (LocalTypedContext->World->OverlapMultiByObjectType(OutOverlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(LocalSettings->CollisionObjectType), CollisionShape, CollisionParams))
				{
					ProcessOverlapResults();
				}
				else { SamplingFailed(); }
				break;
			case EPCGExCollisionFilterType::Profile:
				if (LocalTypedContext->World->OverlapMultiByProfile(OutOverlaps, Origin, FQuat::Identity, LocalSettings->CollisionProfileName, CollisionShape, CollisionParams))
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
		PointDataFacade->Write(AsyncManagerPtr, true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
