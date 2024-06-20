// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleSurfaceGuided.h"

#include "Data/PCGExDataFilter.h"

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

FName UPCGExSampleSurfaceGuidedSettings::GetPointFilterLabel() const { return PCGExDataFilter::SourceFiltersLabel; }

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
				NewBatch->SetPointsFilterData(&Context->FilterFactories);
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
		Context->ExecuteEnd();
	}

	return Context->IsDone();
}

namespace PCGExSampleSurfaceGuided
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(MaxDistanceGetter)
		PCGEX_DELETE(DirectionGetter)

		PCGEX_FOREACH_FIELD_SURFACEGUIDED(PCGEX_OUTPUT_DELETE)
	}

	bool FProcessor::Process(FPCGExAsyncManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SampleSurfaceGuided)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		DirectionGetter = new PCGEx::FLocalVectorGetter();
		DirectionGetter->Capture(Settings->Direction);

		if (!DirectionGetter->Grab(*PointIO))
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Some inputs don't have the required Direction data."));
			return false;
		}

		{
			PCGExData::FPointIO& OutputIO = *PointIO;
			PCGEX_FOREACH_FIELD_SURFACEGUIDED(PCGEX_OUTPUT_FWD_INIT)
		}


		MaxDistanceGetter = new PCGEx::FLocalSingleFieldGetter();

		if (Settings->bUseLocalMaxDistance)
		{
			MaxDistanceGetter->Capture(Settings->LocalMaxDistance);
			if (MaxDistanceGetter->Grab(*PointIO)) { PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("RangeMin metadata missing")); }
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SampleSurfaceGuided)

		const double MaxDistance = MaxDistanceGetter->SafeGet(Index, Settings->MaxDistance);
		const FVector Direction = (*DirectionGetter)[Index].GetSafeNormal();

		auto SamplingFailed = [&]()
		{
			PCGEX_OUTPUT_VALUE(Location, Index, Point.Transform.GetLocation())
			PCGEX_OUTPUT_VALUE(Normal, Index, Direction)
			PCGEX_OUTPUT_VALUE(Distance, Index, MaxDistance)
			PCGEX_OUTPUT_VALUE(Success, Index, false)
		};

		if (!PointFilterCache[Index])
		{
			SamplingFailed();
			return;
		}

		const FVector Origin = Point.Transform.GetLocation();

		FCollisionQueryParams CollisionParams;
		CollisionParams.bTraceComplex = true;
		CollisionParams.AddIgnoredActors(TypedContext->IgnoredActors);

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
			bSuccess = true;
		};

		switch (Settings->CollisionType)
		{
		case EPCGExCollisionFilterType::Channel:
			if (TypedContext->World->LineTraceSingleByChannel(HitResult, Origin, End, Settings->CollisionChannel, CollisionParams))
			{
				ProcessTraceResult();
			}
			break;
		case EPCGExCollisionFilterType::ObjectType:
			if (TypedContext->World->LineTraceSingleByObjectType(HitResult, Origin, End, FCollisionObjectQueryParams(Settings->CollisionObjectType), CollisionParams))
			{
				ProcessTraceResult();
			}
			break;
		case EPCGExCollisionFilterType::Profile:
			if (TypedContext->World->LineTraceSingleByProfile(HitResult, Origin, End, Settings->CollisionProfileName, CollisionParams))
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
		PCGEX_FOREACH_FIELD_SURFACEGUIDED(PCGEX_OUTPUT_WRITE)
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
