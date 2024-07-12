// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleSurfaceGuided.h"

#include "Data/PCGExPointFilter.h"

#define LOCTEXT_NAMESPACE "PCGExSampleSurfaceGuidedElement"
#define PCGEX_NAMESPACE SampleSurfaceGuided

TArray<FPCGPinProperties> UPCGExSampleSurfaceGuidedSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGEx::SourcePointFilters, "Filter which points will be processed.", Advanced, {})
	return PinProperties;
}

PCGExData::EInit UPCGExSampleSurfaceGuidedSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

int32 UPCGExSampleSurfaceGuidedSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_L; }

FName UPCGExSampleSurfaceGuidedSettings::GetPointFilterLabel() const { return PCGExPointFilter::SourceFiltersLabel; }

PCGEX_INITIALIZE_ELEMENT(SampleSurfaceGuided)

FPCGExSampleSurfaceGuidedContext::~FPCGExSampleSurfaceGuidedContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExSampleSurfaceGuidedElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleSurfaceGuided)

	PCGEX_FOREACH_FIELD_SURFACEGUIDED(PCGEX_OUTPUT_VALIDATE_NAME)

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

	if (Context->IsDone())
	{
		Context->OutputMainPoints();
	}

	return Context->TryComplete();
}

namespace PCGExSampleSurfaceGuided
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleSurfaceGuided::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SampleSurfaceGuided)

		LocalTypedContext = TypedContext;
		LocalSettings = Settings;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		DirectionGetter = PointDataFacade->GetOrCreateGetter<FVector>(Settings->Direction);

		if (!DirectionGetter)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Some inputs don't have the required Direction data."));
			return false;
		}

		{
			PCGExData::FFacade* OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_SURFACEGUIDED(PCGEX_OUTPUT_INIT)
		}

		if (Settings->bUseLocalMaxDistance)
		{
			MaxDistanceGetter = PointDataFacade->GetOrCreateGetter<double>(Settings->LocalMaxDistance);
			if (MaxDistanceGetter) { PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("RangeMin metadata missing")); }
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		const double MaxDistance = MaxDistanceGetter ? MaxDistanceGetter->Values[Index] : LocalSettings->MaxDistance;
		const FVector Direction = DirectionGetter->Values[Index].GetSafeNormal();

		auto SamplingFailed = [&]()
		{
			PCGEX_OUTPUT_VALUE(Location, Index, Point.Transform.GetLocation())
			PCGEX_OUTPUT_VALUE(Normal, Index, Direction)
			PCGEX_OUTPUT_VALUE(Distance, Index, MaxDistance)
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
		CollisionParams.AddIgnoredActors(LocalTypedContext->IgnoredActors);

		const FVector Trace = Direction * MaxDistance;
		const FVector End = Origin + Trace;

		bool bSuccess = false;
		FHitResult HitResult;

		auto ProcessTraceResult = [&]()
		{
			PCGEX_OUTPUT_VALUE(Location, Index, HitResult.ImpactPoint)
			PCGEX_OUTPUT_VALUE(Normal, Index, HitResult.Normal)
			PCGEX_OUTPUT_VALUE(Distance, Index, FVector::Distance(HitResult.ImpactPoint, Origin))
			PCGEX_OUTPUT_VALUE(Success, Index, bSuccess)

			if (const AActor* HitActor = HitResult.GetActor()) { PCGEX_OUTPUT_VALUE(ActorReference, Index, HitActor->GetPathName()) }
			else { PCGEX_OUTPUT_VALUE(ActorReference, Index, TEXT("")) }
			if (const UPhysicalMaterial* PhysMat = HitResult.PhysMaterial.Get()) { PCGEX_OUTPUT_VALUE(PhysMat, Index, PhysMat->GetPathName()) }
			else { PCGEX_OUTPUT_VALUE(PhysMat, Index, TEXT("")) }
			bSuccess = true;
		};

		switch (LocalSettings->CollisionType)
		{
		case EPCGExCollisionFilterType::Channel:
			if (LocalTypedContext->World->LineTraceSingleByChannel(HitResult, Origin, End, LocalSettings->CollisionChannel, CollisionParams))
			{
				ProcessTraceResult();
			}
			break;
		case EPCGExCollisionFilterType::ObjectType:
			if (LocalTypedContext->World->LineTraceSingleByObjectType(HitResult, Origin, End, FCollisionObjectQueryParams(LocalSettings->CollisionObjectType), CollisionParams))
			{
				ProcessTraceResult();
			}
			break;
		case EPCGExCollisionFilterType::Profile:
			if (LocalTypedContext->World->LineTraceSingleByProfile(HitResult, Origin, End, LocalSettings->CollisionProfileName, CollisionParams))
			{
				ProcessTraceResult();
			}
			break;
		default:
			SamplingFailed();
			break;
		}

		if (!bSuccess) { SamplingFailed(); }
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManagerPtr, true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
