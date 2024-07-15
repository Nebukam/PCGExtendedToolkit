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

namespace PCGExSampleNearestSurface
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleNearestSurface::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SampleNearestSurface)

		LocalTypedContext = TypedContext;
		LocalSettings = Settings;

		// TODO : Add Scoped Fetch
		
		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		{
			PCGExData::FFacade* OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_NEARESTSURFACE(PCGEX_OUTPUT_INIT)
		}

		if (Settings->bUseLocalMaxDistance)
		{
			MaxDistanceGetter = PointDataFacade->GetScopedBroadcaster<double>(Settings->LocalMaxDistance);
			if (!MaxDistanceGetter) { PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("RangeMin metadata missing")); }
		}

		StartParallelLoopForPoints();

		return true;
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
			PCGEX_OUTPUT_VALUE(Success, Index, false)

			PCGEX_OUTPUT_VALUE(ActorReference, Index, TEXT(""))
			PCGEX_OUTPUT_VALUE(PhysMat, Index, TEXT(""))
		};

		if (!PointFilterCache[Index])
		{
			SamplingFailed();
			return;
		}

		const FVector Origin = PointIO->GetInPoint(Index).Transform.GetLocation();

		FCollisionQueryParams CollisionParams;
		CollisionParams.bTraceComplex = false;
		CollisionParams.AddIgnoredActors(LocalTypedContext->IgnoredActors);

		const FCollisionShape CollisionShape = FCollisionShape::MakeSphere(MaxDistance);

		FVector HitLocation;
		bool bSuccess = false;
		TArray<FOverlapResult> OutOverlaps;

		auto ProcessOverlapResults = [&]()
		{
			float MinDist = MAX_FLT;
			UPrimitiveComponent* HitComp = nullptr;
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
					HitComp = Overlap.Component.Get();
				}
			}

			if (bSuccess)
			{
				const FVector Direction = (HitLocation - Origin).GetSafeNormal();
				PCGEX_OUTPUT_VALUE(Location, Index, HitLocation)
				PCGEX_OUTPUT_VALUE(Normal, Index, Direction*-1) // TODO: expose "precise normal" in which case we line trace to location
				PCGEX_OUTPUT_VALUE(LookAt, Index, Direction)
				PCGEX_OUTPUT_VALUE(Distance, Index, MinDist)

				if (HitComp)
				{
					PCGEX_OUTPUT_VALUE(ActorReference, Index, HitComp->GetOwner()->GetPathName())
					UPhysicalMaterial* PhysMat = HitComp->GetBodyInstance()->GetSimplePhysicalMaterial();
					if (PhysMat) { PCGEX_OUTPUT_VALUE(PhysMat, Index, PhysMat->GetPathName()) }
					else { PCGEX_OUTPUT_VALUE(PhysMat, Index, TEXT("")) }
				}
				else
				{
					PCGEX_OUTPUT_VALUE(ActorReference, Index, TEXT(""))
					PCGEX_OUTPUT_VALUE(PhysMat, Index, TEXT(""))
				}
			}
			else
			{
				SamplingFailed();
			}
		};


		switch (LocalSettings->CollisionType)
		{
		case EPCGExCollisionFilterType::Channel:
			if (LocalTypedContext->World->OverlapMultiByChannel(OutOverlaps, Origin, FQuat::Identity, LocalSettings->CollisionChannel, CollisionShape, CollisionParams))
			{
				ProcessOverlapResults();
			}
			break;
		case EPCGExCollisionFilterType::ObjectType:
			if (LocalTypedContext->World->OverlapMultiByObjectType(OutOverlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(LocalSettings->CollisionObjectType), CollisionShape, CollisionParams))
			{
				ProcessOverlapResults();
			}
			break;
		case EPCGExCollisionFilterType::Profile:
			if (LocalTypedContext->World->OverlapMultiByProfile(OutOverlaps, Origin, FQuat::Identity, LocalSettings->CollisionProfileName, CollisionShape, CollisionParams))
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
		PointDataFacade->Write(AsyncManagerPtr, true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
