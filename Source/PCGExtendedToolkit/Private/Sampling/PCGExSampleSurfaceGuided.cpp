// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleSurfaceGuided.h"

#define LOCTEXT_NAMESPACE "PCGExSampleSurfaceGuidedElement"
#define PCGEX_NAMESPACE SampleSurfaceGuided

PCGExData::EInit UPCGExSampleSurfaceGuidedSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

int32 UPCGExSampleSurfaceGuidedSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_L; }

PCGEX_INITIALIZE_ELEMENT(SampleSurfaceGuided)

FPCGExSampleSurfaceGuidedContext::~FPCGExSampleSurfaceGuidedContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(MaxDistanceGetter)
	PCGEX_DELETE(DirectionGetter)

	PCGEX_FOREACH_FIELD_SURFACEGUIDED(PCGEX_OUTPUT_DELETE)
}

bool FPCGExSampleSurfaceGuidedElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleSurfaceGuided)

	PCGEX_FWD(CollisionChannel)
	PCGEX_FWD(CollisionObjectType)
	PCGEX_FWD(CollisionProfileName)
	PCGEX_FWD(bIgnoreSelf)

	PCGEX_FWD(MaxDistance)
	PCGEX_FWD(bUseLocalMaxDistance)

	Context->MaxDistanceGetter = new PCGEx::FLocalSingleFieldGetter();
	Context->MaxDistanceGetter->Capture(Settings->LocalMaxDistance);

	Context->DirectionGetter = new PCGEx::FLocalVectorGetter();
	Context->DirectionGetter->Capture(Settings->Direction);

	PCGEX_FOREACH_FIELD_SURFACEGUIDED(PCGEX_OUTPUT_FWD)

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

		if (Context->bIgnoreSelf) { Context->IgnoredActors.Add(Context->SourceComponent->GetOwner()); }

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
		else
		{
			PCGExData::FPointIO& PointIO = *Context->CurrentIO;
			PointIO.CreateOutKeys();

			if (!Context->DirectionGetter->Grab(PointIO))
			{
				PCGE_LOG(Error, GraphAndLog, FTEXT("Some inputs don't have the desired Direction data."));
				return false;
			}

			if (Settings->bUseLocalMaxDistance)
			{
				if (!Context->MaxDistanceGetter->Grab(PointIO))
				{
					PCGE_LOG(Error, GraphAndLog, FTEXT("Some inputs don't have the desired Local Max Distance data."));
				}
			}

			PCGEX_FOREACH_FIELD_SURFACEGUIDED(PCGEX_OUTPUT_ACCESSOR_INIT)

			for (int i = 0; i < PointIO.GetNum(); i++) { Context->GetAsyncManager()->Start<FTraceTask>(i, Context->CurrentIO); }
			Context->SetAsyncState(PCGExMT::State_ProcessingPoints);
		}
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		PCGEX_WAIT_ASYNC

		PCGEX_FOREACH_FIELD_SURFACEGUIDED(PCGEX_OUTPUT_WRITE)
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone()) { Context->OutputPoints(); }

	return Context->IsDone();
}

bool FTraceTask::ExecuteTask()
{
	const FPCGExSampleSurfaceGuidedContext* Context = Manager->GetContext<FPCGExSampleSurfaceGuidedContext>();


	const FVector Origin = PointIO->GetInPoint(TaskIndex).Transform.GetLocation();

	FCollisionQueryParams CollisionParams;
	CollisionParams.bTraceComplex = true;
	CollisionParams.AddIgnoredActors(Context->IgnoredActors);

	const double MaxDistance = Context->MaxDistanceGetter->bValid ? (*Context->MaxDistanceGetter)[TaskIndex] : Context->MaxDistance;
	const FVector Trace = (*Context->DirectionGetter)[TaskIndex] * MaxDistance;
	const FVector End = Origin + Trace;

	bool bSuccess = false;
	FHitResult HitResult;

	auto ProcessTraceResult = [&]()
	{
		PCGEX_OUTPUT_VALUE(Location, TaskIndex, HitResult.ImpactPoint)
		PCGEX_OUTPUT_VALUE(Normal, TaskIndex, HitResult.Normal)
		PCGEX_OUTPUT_VALUE(Distance, TaskIndex, FVector::Distance(HitResult.ImpactPoint, Origin))
		bSuccess = true;
	};


	switch (Context->CollisionType)
	{
	case EPCGExCollisionFilterType::Channel:
		if (Context->World->LineTraceSingleByChannel(HitResult, Origin, End, Context->CollisionChannel, CollisionParams))
		{
			ProcessTraceResult();
		}
		break;
	case EPCGExCollisionFilterType::ObjectType:
		if (Context->World->LineTraceSingleByObjectType(HitResult, Origin, End, FCollisionObjectQueryParams(Context->CollisionObjectType), CollisionParams))
		{
			ProcessTraceResult();
		}
		break;
	case EPCGExCollisionFilterType::Profile:
		if (Context->World->LineTraceSingleByProfile(HitResult, Origin, End, Context->CollisionProfileName, CollisionParams))
		{
			ProcessTraceResult();
		}
		break;
	default: ;
	}


	if (!bSuccess)
	{
		PCGEX_OUTPUT_VALUE(Location, TaskIndex, End)
		PCGEX_OUTPUT_VALUE(Normal, TaskIndex, Trace.GetSafeNormal()*-1)
		PCGEX_OUTPUT_VALUE(Distance, TaskIndex, MaxDistance)
	}

	PCGEX_OUTPUT_VALUE(Success, TaskIndex, bSuccess)
	return bSuccess;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
