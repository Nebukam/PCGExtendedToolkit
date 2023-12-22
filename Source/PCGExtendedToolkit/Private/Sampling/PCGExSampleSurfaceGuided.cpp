// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleSurfaceGuided.h"

#define LOCTEXT_NAMESPACE "PCGExSampleSurfaceGuidedElement"
#define PCGEX_NAMESPACE SampleSurfaceGuided

PCGExData::EInit UPCGExSampleSurfaceGuidedSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

int32 UPCGExSampleSurfaceGuidedSettings::GetPreferredChunkSize() const { return 32; }

FPCGElementPtr UPCGExSampleSurfaceGuidedSettings::CreateElement() const { return MakeShared<FPCGExSampleSurfaceGuidedElement>(); }

FPCGExSampleSurfaceGuidedContext::~FPCGExSampleSurfaceGuidedContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_SAMPLENEARESTTRACE_FOREACH(PCGEX_OUTPUT_DELETE)
}

PCGEX_INITIALIZE_CONTEXT(SampleSurfaceGuided)

bool FPCGExSampleSurfaceGuidedElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleSurfaceGuided)

	PCGEX_FWD(CollisionChannel)
	PCGEX_FWD(CollisionObjectType)
	PCGEX_FWD(CollisionProfileName)
	PCGEX_FWD(bIgnoreSelf)

	PCGEX_FWD(Size)
	PCGEX_FWD(bUseLocalSize)
	PCGEX_FWD(bProjectFailToSize)

	Context->SizeGetter.Capture(Settings->LocalSize);
	Context->DirectionGetter.Capture(Settings->Direction);

	PCGEX_SAMPLENEARESTTRACE_FOREACH(PCGEX_OUTPUT_FWD)

	PCGEX_SAMPLENEARESTTRACE_FOREACH(PCGEX_OUTPUT_VALIDATE_NAME)

	return true;
}

bool FPCGExSampleSurfaceGuidedElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleSurfaceGuidedElement::Execute);

	PCGEX_CONTEXT(SampleSurfaceGuided)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		if (Context->bIgnoreSelf) { Context->IgnoredActors.Add(Context->SourceComponent->GetOwner()); }
		const UPCGExSampleSurfaceGuidedSettings* Settings = Context->GetInputSettings<UPCGExSampleSurfaceGuidedSettings>();
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
			Context->DirectionGetter.Bind(PointIO);

			PCGEX_SAMPLENEARESTTRACE_FOREACH(PCGEX_OUTPUT_ACCESSOR_INIT)
		};

		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			Context->GetAsyncManager()->Start<FTraceTask>(PointIndex, Context->CurrentIO);
		};

		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint)) { Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork); }
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (Context->IsAsyncWorkComplete())
		{
			PCGEX_SAMPLENEARESTTRACE_FOREACH(PCGEX_OUTPUT_WRITE)
			Context->CurrentIO->OutputTo(Context);
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
	}

	return Context->IsDone();
}

bool FTraceTask::ExecuteTask()
{
	const FPCGExSampleSurfaceGuidedContext* Context = Manager->GetContext<FPCGExSampleSurfaceGuidedContext>();
	

	const FVector Origin = PointIO->GetInPoint(TaskIndex).Transform.GetLocation();

	FCollisionQueryParams CollisionParams;
	CollisionParams.bTraceComplex = true;
	CollisionParams.AddIgnoredActors(Context->IgnoredActors);

	const double Size = Context->bUseLocalSize ? Context->SizeGetter[TaskIndex] : Context->Size;
	const FVector Trace = Context->DirectionGetter[TaskIndex] * Size;
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

	
	if (Context->bProjectFailToSize)
	{
		PCGEX_OUTPUT_VALUE(Location, TaskIndex, End)
		PCGEX_OUTPUT_VALUE(Normal, TaskIndex, Trace.GetSafeNormal()*-1)
		PCGEX_OUTPUT_VALUE(Distance, TaskIndex, Size)
	}

	PCGEX_OUTPUT_VALUE(Success, TaskIndex, bSuccess)
	return bSuccess;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
