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

PCGEX_INITIALIZE_ELEMENT(SampleSurfaceGuided)

FPCGExSampleSurfaceGuidedContext::~FPCGExSampleSurfaceGuidedContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(PointFilterManager)
	PointFilterFactories.Empty();

	PCGEX_DELETE(MaxDistanceGetter)
	PCGEX_DELETE(DirectionGetter)

	PCGEX_FOREACH_FIELD_SURFACEGUIDED(PCGEX_OUTPUT_DELETE)
}

bool FPCGExSampleSurfaceGuidedElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleSurfaceGuided)

	Context->MaxDistanceGetter = new PCGEx::FLocalSingleFieldGetter();
	Context->MaxDistanceGetter->Capture(Settings->LocalMaxDistance);

	Context->DirectionGetter = new PCGEx::FLocalVectorGetter();
	Context->DirectionGetter->Capture(Settings->Direction);

	PCGEX_FOREACH_FIELD_SURFACEGUIDED(PCGEX_OUTPUT_FWD)

	PCGEX_FOREACH_FIELD_SURFACEGUIDED(PCGEX_OUTPUT_VALIDATE_NAME)

	PCGExFactories::GetInputFactories(InContext, PCGEx::SourcePointFilters, Context->PointFilterFactories, {PCGExFactories::EType::Filter}, false);

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

		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
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

		for (int i = 0; i < PointIO.GetNum(); i++)
		{
			if (Context->PointFilterManager && !Context->PointFilterManager->Results[i]) { continue; }
			Context->GetAsyncManager()->Start<FTraceTask>(i, Context->CurrentIO);
		}

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC

		PCGEX_FOREACH_FIELD_SURFACEGUIDED(PCGEX_OUTPUT_WRITE)
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}

bool FTraceTask::ExecuteTask()
{
	const FPCGExSampleSurfaceGuidedContext* Context = Manager->GetContext<FPCGExSampleSurfaceGuidedContext>();
	PCGEX_SETTINGS(SampleSurfaceGuided)

	const FVector Origin = PointIO->GetInPoint(TaskIndex).Transform.GetLocation();

	FCollisionQueryParams CollisionParams;
	CollisionParams.bTraceComplex = true;
	CollisionParams.AddIgnoredActors(Context->IgnoredActors);

	const double MaxDistance = Context->MaxDistanceGetter->SafeGet(TaskIndex, Settings->MaxDistance);
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


	switch (Settings->CollisionType)
	{
	case EPCGExCollisionFilterType::Channel:
		if (Context->World->LineTraceSingleByChannel(HitResult, Origin, End, Settings->CollisionChannel, CollisionParams))
		{
			ProcessTraceResult();
		}
		break;
	case EPCGExCollisionFilterType::ObjectType:
		if (Context->World->LineTraceSingleByObjectType(HitResult, Origin, End, FCollisionObjectQueryParams(Settings->CollisionObjectType), CollisionParams))
		{
			ProcessTraceResult();
		}
		break;
	case EPCGExCollisionFilterType::Profile:
		if (Context->World->LineTraceSingleByProfile(HitResult, Origin, End, Settings->CollisionProfileName, CollisionParams))
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
