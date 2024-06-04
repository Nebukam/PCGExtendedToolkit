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

PCGEX_INITIALIZE_ELEMENT(SampleNearestSurface)

FPCGExSampleNearestSurfaceContext::~FPCGExSampleNearestSurfaceContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(PointFilterManager)
	PointFilterFactories.Empty();

	PCGEX_DELETE(MaxDistanceGetter)

	PCGEX_FOREACH_FIELD_NEARESTSURFACE(PCGEX_OUTPUT_DELETE)
}

bool FPCGExSampleNearestSurfaceElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestSurface)

	Context->MaxDistanceGetter = new PCGEx::FLocalSingleFieldGetter();
	Context->MaxDistanceGetter->Capture(Settings->LocalMaxDistance);

	PCGEX_FOREACH_FIELD_NEARESTSURFACE(PCGEX_OUTPUT_FWD)

	PCGEX_FOREACH_FIELD_NEARESTSURFACE(PCGEX_OUTPUT_VALIDATE_NAME)

	PCGExFactories::GetInputFactories(InContext, PCGEx::SourcePointFilters, Context->PointFilterFactories, {PCGExFactories::EType::Filter}, false);

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

		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			Context->Done();
			Context->ExecutionComplete();
		}
		else
		{
			if (!Context->PointFilterFactories.IsEmpty()) { Context->SetState(PCGExDataFilter::State_FilteringPoints); }
			else { Context->SetState(PCGExMT::State_ProcessingPoints); }
		}
	}

	if (Context->IsState(PCGExDataFilter::State_FilteringPoints))
	{
		auto Initialize = [&](const PCGExData::FPointIO& PointIO)
		{
			PCGEX_DELETE(Context->PointFilterManager)
			Context->PointFilterManager = new PCGExDataFilter::TEarlyExitFilterManager(&PointIO);
			Context->PointFilterManager->Register<UPCGExFilterFactoryBase>(Context, Context->PointFilterFactories, &PointIO);
		};

		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO) { Context->PointFilterManager->Test(PointIndex); };

		if (!Context->ProcessCurrentPoints(Initialize, ProcessPoint)) { return false; }

		Context->SetState(PCGExMT::State_ProcessingPoints);
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		PCGExData::FPointIO& PointIO = *Context->CurrentIO;
		PointIO.CreateOutKeys();

		if (Settings->bUseLocalMaxDistance)
		{
			if (!Context->MaxDistanceGetter->Grab(PointIO))
			{
				PCGE_LOG(Error, GraphAndLog, FTEXT("Some inputs don't have the desired Local Max Distance data."));
			}
		}

		PCGEX_FOREACH_FIELD_NEARESTSURFACE(PCGEX_OUTPUT_ACCESSOR_INIT)

		for (int i = 0; i < PointIO.GetNum(); i++)
		{
			if (Context->PointFilterManager && !Context->PointFilterManager->Results[i]) { continue; }
			Context->GetAsyncManager()->Start<FSweepSphereTask>(i, Context->CurrentIO);
		}

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC

		PCGEX_FOREACH_FIELD_NEARESTSURFACE(PCGEX_OUTPUT_WRITE)
		Context->CurrentIO->OutputTo(Context);
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	return Context->IsDone();
}

bool FSweepSphereTask::ExecuteTask()
{
	const FPCGExSampleNearestSurfaceContext* Context = Manager->GetContext<FPCGExSampleNearestSurfaceContext>();
	PCGEX_SETTINGS(SampleNearestSurface)

	const FVector Origin = PointIO->GetInPoint(TaskIndex).Transform.GetLocation();

	FCollisionQueryParams CollisionParams;
	CollisionParams.bTraceComplex = false;
	CollisionParams.AddIgnoredActors(Context->IgnoredActors);

	const double MaxDistance = Context->MaxDistanceGetter->bValid ? (*Context->MaxDistanceGetter)[TaskIndex] : Settings->MaxDistance;
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
			PCGEX_ASYNC_CHECKPOINT_VOID
			const FVector Direction = (HitLocation - Origin).GetSafeNormal();
			PCGEX_OUTPUT_VALUE(Location, TaskIndex, HitLocation)
			PCGEX_OUTPUT_VALUE(Normal, TaskIndex, Direction*-1) // TODO: expose "precise normal" in which case we line trace to location
			PCGEX_OUTPUT_VALUE(LookAt, TaskIndex, Direction)
			PCGEX_OUTPUT_VALUE(Distance, TaskIndex, MinDist)
		}
	};


	switch (Settings->CollisionType)
	{
	case EPCGExCollisionFilterType::Channel:
		if (Context->World->OverlapMultiByChannel(OutOverlaps, Origin, FQuat::Identity, Settings->CollisionChannel, CollisionShape, CollisionParams))
		{
			ProcessOverlapResults();
		}
		break;
	case EPCGExCollisionFilterType::ObjectType:
		if (Context->World->OverlapMultiByObjectType(OutOverlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(Settings->CollisionObjectType), CollisionShape, CollisionParams))
		{
			ProcessOverlapResults();
		}
		break;
	case EPCGExCollisionFilterType::Profile:
		if (Context->World->OverlapMultiByProfile(OutOverlaps, Origin, FQuat::Identity, Settings->CollisionProfileName, CollisionShape, CollisionParams))
		{
			ProcessOverlapResults();
		}
		break;
	default: ;
	}


	PCGEX_OUTPUT_VALUE(Success, TaskIndex, bSuccess)
	return bSuccess;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
