// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestSurface.h"

#define LOCTEXT_NAMESPACE "PCGExSampleNearestSurfaceElement"
#define PCGEX_NAMESPACE SampleNearestSurface

PCGExData::EInit UPCGExSampleNearestSurfaceSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

int32 UPCGExSampleNearestSurfaceSettings::GetPreferredChunkSize() const { return 32; }

PCGEX_INITIALIZE_ELEMENT(SampleNearestSurface)

FPCGExSampleNearestSurfaceContext::~FPCGExSampleNearestSurfaceContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_SAMPLENEARESTSURFACE_FOREACH(PCGEX_OUTPUT_DELETE)
}

bool FPCGExSampleNearestSurfaceElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestSurface)

	Context->RangeMax = Settings->MaxDistance;

	PCGEX_FWD(CollisionType)
	PCGEX_FWD(CollisionChannel)
	PCGEX_FWD(CollisionObjectType)
	PCGEX_FWD(CollisionProfileName)
	PCGEX_FWD(bIgnoreSelf)

	PCGEX_SAMPLENEARESTSURFACE_FOREACH(PCGEX_OUTPUT_FWD)

	PCGEX_SAMPLENEARESTSURFACE_FOREACH(PCGEX_OUTPUT_VALIDATE_NAME)

	return true;
}

bool FPCGExSampleNearestSurfaceElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestSurfaceElement::Execute);

	PCGEX_CONTEXT(SampleNearestSurface)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		if (Context->bIgnoreSelf) { Context->IgnoredActors.Add(Context->SourceComponent->GetOwner()); }

		const UPCGExSampleNearestSurfaceSettings* Settings = Context->GetInputSettings<UPCGExSampleNearestSurfaceSettings>();
		check(Settings);

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
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else { Context->SetState(PCGExMT::State_ProcessingPoints); }
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		auto Initialize = [&](PCGExData::FPointIO& PointIO)
		{
			PointIO.CreateOutKeys();
			PCGEX_SAMPLENEARESTSURFACE_FOREACH(PCGEX_OUTPUT_ACCESSOR_INIT)
		};

		auto ProcessPoint = [&](int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			Context->GetAsyncManager()->Start<FSweepSphereTask>(PointIndex, Context->CurrentIO);
		};

		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint)) { Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork); }
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (Context->IsAsyncWorkComplete())
		{
			PCGEX_SAMPLENEARESTSURFACE_FOREACH(PCGEX_OUTPUT_WRITE)
			Context->CurrentIO->OutputTo(Context);
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
	}

	return Context->IsDone();
}

bool FSweepSphereTask::ExecuteTask()
{
	const FPCGExSampleNearestSurfaceContext* Context = Manager->GetContext<FPCGExSampleNearestSurfaceContext>();


	const FVector Origin = PointIO->GetInPoint(TaskIndex).Transform.GetLocation();

	FCollisionQueryParams CollisionParams;
	CollisionParams.bTraceComplex = false;
	CollisionParams.AddIgnoredActors(Context->IgnoredActors);

	const FCollisionShape CollisionShape = FCollisionShape::MakeSphere(Context->RangeMax);

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


	switch (Context->CollisionType)
	{
	case EPCGExCollisionFilterType::Channel:
		if (Context->World->OverlapMultiByChannel(OutOverlaps, Origin, FQuat::Identity, Context->CollisionChannel, CollisionShape, CollisionParams))
		{
			ProcessOverlapResults();
		}
		break;
	case EPCGExCollisionFilterType::ObjectType:
		if (Context->World->OverlapMultiByObjectType(OutOverlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(Context->CollisionObjectType), CollisionShape, CollisionParams))
		{
			ProcessOverlapResults();
		}
		break;
	case EPCGExCollisionFilterType::Profile:
		if (Context->World->OverlapMultiByProfile(OutOverlaps, Origin, FQuat::Identity, Context->CollisionProfileName, CollisionShape, CollisionParams))
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
