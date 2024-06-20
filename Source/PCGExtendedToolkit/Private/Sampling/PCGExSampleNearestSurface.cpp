// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestSurface.h"

#include "Data/PCGExDataFilter.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
#include "Engine/OverlapResult.h"
#endif

#define LOCTEXT_NAMESPACE "PCGExSampleNearestSurfaceElement"
#define PCGEX_NAMESPACE SampleNearestSurface

TArray<FPCGPinProperties> UPCGExSampleNearestSurfaceSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGEx::SourcePointFilters, "Filter which points will be processed.", Advanced, {})
	return PinProperties;
}

PCGExData::EInit UPCGExSampleNearestSurfaceSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

int32 UPCGExSampleNearestSurfaceSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_L; }

FName UPCGExSampleNearestSurfaceSettings::GetPointFilterLabel() const { return PCGExDataFilter::SourceFiltersLabel; }

PCGEX_INITIALIZE_ELEMENT(SampleNearestSurface)

FPCGExSampleNearestSurfaceContext::~FPCGExSampleNearestSurfaceContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExSampleNearestSurfaceElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestSurface)

	PCGEX_FOREACH_FIELD_NEARESTSURFACE(PCGEX_OUTPUT_VALIDATE_NAME)

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

namespace PCGExSampleNearestSurface
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(MaxDistanceGetter)

		PCGEX_FOREACH_FIELD_NEARESTSURFACE(PCGEX_OUTPUT_DELETE)
	}

	bool FProcessor::Process(FPCGExAsyncManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SampleNearestSurface)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		{
			PCGExData::FPointIO& OutputIO = *PointIO;
			PCGEX_FOREACH_FIELD_NEARESTSURFACE(PCGEX_OUTPUT_FWD_INIT)
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
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SampleNearestSurface)

		const double MaxDistance = MaxDistanceGetter->SafeGet(Index, Settings->MaxDistance);

		auto SamplingFailed = [&]()
		{
			const FVector Direction = FVector::UpVector;
			PCGEX_OUTPUT_VALUE(Location, Index, Point.Transform.GetLocation())
			PCGEX_OUTPUT_VALUE(Normal, Index, Direction*-1) // TODO: expose "precise normal" in which case we line trace to location
			PCGEX_OUTPUT_VALUE(LookAt, Index, Direction)
			PCGEX_OUTPUT_VALUE(Distance, Index, MaxDistance)
			PCGEX_OUTPUT_VALUE(Success, Index, false)
		};

		if (!PointFilterCache[Index])
		{
			SamplingFailed();
			return;
		}

		const FVector Origin = PointIO->GetInPoint(Index).Transform.GetLocation();

		FCollisionQueryParams CollisionParams;
		CollisionParams.bTraceComplex = false;
		CollisionParams.AddIgnoredActors(TypedContext->IgnoredActors);

		const FCollisionShape CollisionShape = FCollisionShape::MakeSphere(MaxDistance);

		FVector HitLocation;
		bool bSuccess = false;
		TArray<FOverlapResult> OutOverlaps;

		auto ProcessOverlapResults = [&]()
		{
			float MinDist = MAX_FLT;
			for (const FOverlapResult& Overlap : OutOverlaps)
			{
				if (!Overlap.bBlockingHit) { continue; }
				FVector OutClosestLocation;
				const float Distance = Overlap.Component->GetClosestPointOnCollision(Origin, OutClosestLocation);
				if (Distance < 0) { continue; }
				if (Distance == 0)
				{
					// Fallback for complex collisions?
					continue;
				}
				if (Distance < MinDist)
				{
					MinDist = Distance;
					HitLocation = OutClosestLocation;
					bSuccess = true;
				}
			}

			if (bSuccess)
			{
				const FVector Direction = (HitLocation - Origin).GetSafeNormal();
				PCGEX_OUTPUT_VALUE(Location, Index, HitLocation)
				PCGEX_OUTPUT_VALUE(Normal, Index, Direction*-1) // TODO: expose "precise normal" in which case we line trace to location
				PCGEX_OUTPUT_VALUE(LookAt, Index, Direction)
				PCGEX_OUTPUT_VALUE(Distance, Index, MinDist)
			}
			else
			{
				SamplingFailed();
			}
		};


		switch (Settings->CollisionType)
		{
		case EPCGExCollisionFilterType::Channel:
			if (TypedContext->World->OverlapMultiByChannel(OutOverlaps, Origin, FQuat::Identity, Settings->CollisionChannel, CollisionShape, CollisionParams))
			{
				ProcessOverlapResults();
			}
			break;
		case EPCGExCollisionFilterType::ObjectType:
			if (TypedContext->World->OverlapMultiByObjectType(OutOverlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(Settings->CollisionObjectType), CollisionShape, CollisionParams))
			{
				ProcessOverlapResults();
			}
			break;
		case EPCGExCollisionFilterType::Profile:
			if (TypedContext->World->OverlapMultiByProfile(OutOverlaps, Origin, FQuat::Identity, Settings->CollisionProfileName, CollisionShape, CollisionParams))
			{
				ProcessOverlapResults();
			}
			break;
		default:
			SamplingFailed();
			break;
		}
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_FOREACH_FIELD_NEARESTSURFACE(PCGEX_OUTPUT_WRITE)
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
