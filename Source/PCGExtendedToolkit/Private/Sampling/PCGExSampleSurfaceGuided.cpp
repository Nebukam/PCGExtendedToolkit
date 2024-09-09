// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleSurfaceGuided.h"

#include "Data/PCGExPointFilter.h"

#define LOCTEXT_NAMESPACE "PCGExSampleSurfaceGuidedElement"
#define PCGEX_NAMESPACE SampleSurfaceGuided

TArray<FPCGPinProperties> UPCGExSampleSurfaceGuidedSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (SurfaceSource == EPCGExSurfaceSource::ActorReferences) { PCGEX_PIN_POINT(PCGExSampling::SourceActorReferencesLabel, "Points with actor reference paths.", Required, {}) }
	PCGEX_PIN_PARAMS(PCGEx::SourcePointFilters, "Filter which points will be processed.", Advanced, {})
	return PinProperties;
}

PCGExData::EInit UPCGExSampleSurfaceGuidedSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

int32 UPCGExSampleSurfaceGuidedSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_L; }

PCGEX_INITIALIZE_ELEMENT(SampleSurfaceGuided)

FPCGExSampleSurfaceGuidedContext::~FPCGExSampleSurfaceGuidedContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExSampleSurfaceGuidedElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleSurfaceGuided)

	PCGEX_FOREACH_FIELD_SURFACEGUIDED(PCGEX_OUTPUT_VALIDATE_NAME)
	PCGEX_FOREACH_FIELD_SURFACEGUIDED_ACTOR(PCGEX_OUTPUT_VALIDATE_NAME)

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
	}

	return true;
}

bool FPCGExSampleSurfaceGuidedElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleSurfaceGuidedElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleSurfaceGuided)

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

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSampleSurfaceGuided::FProcessor>>(
			[&](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExSampleSurfaceGuided::FProcessor>* NewBatch)
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

namespace PCGExSampleSurfaceGuided
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(SurfacesForward)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleSurfaceGuided::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SampleSurfaceGuided)

		LocalTypedContext = TypedContext;
		LocalSettings = Settings;

		SurfacesForward = TypedContext->bUseInclude ? Settings->AttributesForwarding.TryGetHandler(TypedContext->ActorReferenceDataFacade, PointDataFacade) : nullptr;

		// Must be set before process for filters
		PointDataFacade->bSupportsDynamic = true;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		DirectionGetter = PointDataFacade->GetScopedBroadcaster<FVector>(Settings->Direction);

		if (!DirectionGetter)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Some inputs don't have the required Direction data."));
			return false;
		}

		{
			PCGExData::FFacade* OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_SURFACEGUIDED(PCGEX_OUTPUT_INIT)
			PCGEX_FOREACH_FIELD_SURFACEGUIDED_ACTOR(PCGEX_OUTPUT_INIT_DEFAULT)
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
		FilterScope(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		const double MaxDistance = MaxDistanceGetter ? MaxDistanceGetter->Values[Index] : LocalSettings->MaxDistance;
		const FVector Direction = DirectionGetter->Values[Index].GetSafeNormal();

		auto SamplingFailed = [&]()
		{
			PCGEX_OUTPUT_VALUE(Location, Index, Point.Transform.GetLocation())
			PCGEX_OUTPUT_VALUE(Normal, Index, Direction*-1)
			PCGEX_OUTPUT_VALUE(LookAt, Index, Direction)
			PCGEX_OUTPUT_VALUE(Distance, Index, MaxDistance)
			PCGEX_OUTPUT_VALUE(IsInside, Index, false)
			PCGEX_OUTPUT_VALUE(Success, Index, false)

			PCGEX_OUTPUT_VALUE(ActorReference, Index, TEXT(""))
			PCGEX_OUTPUT_VALUE(PhysMat, Index, TEXT(""))
		};

		if (!PointFilterCache[Index])
		{
			SamplingFailed();
			return;
		}

		const FVector Origin = Point.Transform.GetLocation();

		FCollisionQueryParams CollisionParams;
		CollisionParams.bTraceComplex = LocalSettings->bTraceComplex;
		CollisionParams.bReturnPhysicalMaterial = PhysMatWriter ? true : false;
		CollisionParams.AddIgnoredActors(LocalTypedContext->IgnoredActors);

		const FVector Trace = Direction * MaxDistance;
		const FVector End = Origin + Trace;

		bool bSuccess = false;
		int32* HitIndex = nullptr;
		FHitResult HitResult;
		TArray<FHitResult> HitResults;

		auto ProcessTraceResult = [&]()
		{
			bSuccess = true;

			PCGEX_OUTPUT_VALUE(Location, Index, HitResult.ImpactPoint)
			PCGEX_OUTPUT_VALUE(LookAt, Index, Direction)
			PCGEX_OUTPUT_VALUE(Normal, Index, HitResult.ImpactNormal)
			PCGEX_OUTPUT_VALUE(Distance, Index, FVector::Distance(HitResult.ImpactPoint, Origin))
			PCGEX_OUTPUT_VALUE(IsInside, Index, FVector::DotProduct(Direction, HitResult.ImpactNormal) > 0)
			PCGEX_OUTPUT_VALUE(Success, Index, bSuccess)

			if (const AActor* HitActor = HitResult.GetActor())
			{
				HitIndex = LocalTypedContext->IncludedActors.Find(HitActor);
				PCGEX_OUTPUT_VALUE(ActorReference, Index, HitActor->GetPathName())
			}
			else { PCGEX_OUTPUT_VALUE(ActorReference, Index, TEXT("")) }

			if (const UPhysicalMaterial* PhysMat = HitResult.PhysMaterial.Get()) { PCGEX_OUTPUT_VALUE(PhysMat, Index, PhysMat->GetPathName()) }
			else { PCGEX_OUTPUT_VALUE(PhysMat, Index, TEXT("")) }

			if (SurfacesForward && HitIndex) { SurfacesForward->Forward(*HitIndex, Index); }

			FPlatformAtomics::InterlockedExchange(&bAnySuccess, 1);
		};

		auto ProcessMultipleTraceResult = [&]()
		{
			for (const FHitResult& Hit : HitResults)
			{
				if (LocalTypedContext->IncludedActors.Contains(Hit.GetActor()))
				{
					HitResult = Hit;
					ProcessTraceResult();
					return;
				}
			}
		};

		switch (LocalSettings->CollisionType)
		{
		case EPCGExCollisionFilterType::Channel:
			if (LocalTypedContext->bUseInclude)
			{
				if (LocalTypedContext->World->LineTraceMultiByChannel(
					HitResults, Origin, End,
					LocalSettings->CollisionChannel, CollisionParams))
				{
					ProcessMultipleTraceResult();
				}
			}
			else
			{
				if (LocalTypedContext->World->LineTraceSingleByChannel(
					HitResult, Origin, End,
					LocalSettings->CollisionChannel, CollisionParams))
				{
					ProcessTraceResult();
				}
			}
			break;
		case EPCGExCollisionFilterType::ObjectType:
			if (LocalTypedContext->bUseInclude)
			{
				if (LocalTypedContext->World->LineTraceMultiByObjectType(
					HitResults, Origin, End,
					FCollisionObjectQueryParams(LocalSettings->CollisionObjectType), CollisionParams))
				{
					ProcessMultipleTraceResult();
				}
			}
			else
			{
				if (LocalTypedContext->World->LineTraceSingleByObjectType(
					HitResult, Origin, End,
					FCollisionObjectQueryParams(LocalSettings->CollisionObjectType), CollisionParams)) { ProcessTraceResult(); }
			}
			break;
		case EPCGExCollisionFilterType::Profile:
			if (LocalTypedContext->bUseInclude)
			{
				if (LocalTypedContext->World->LineTraceMultiByProfile(
					HitResults, Origin, End,
					LocalSettings->CollisionProfileName, CollisionParams))
				{
					ProcessMultipleTraceResult();
				}
			}
			else
			{
				if (LocalTypedContext->World->LineTraceSingleByProfile(
					HitResult, Origin, End,
					LocalSettings->CollisionProfileName, CollisionParams)) { ProcessTraceResult(); }
			}
			break;
		default:
			break;
		}

		if (!bSuccess) { SamplingFailed(); }
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManagerPtr, true);

		if (LocalSettings->bTagIfHasSuccesses && bAnySuccess) { PointIO->Tags->Add(LocalSettings->HasSuccessesTag); }
		if (LocalSettings->bTagIfHasNoSuccesses && !bAnySuccess) { PointIO->Tags->Add(LocalSettings->HasNoSuccessesTag); }
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
