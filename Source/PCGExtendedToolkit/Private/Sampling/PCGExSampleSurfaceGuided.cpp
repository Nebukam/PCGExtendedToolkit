// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleSurfaceGuided.h"
#include "Elements/PCGActorSelector.h"

#define LOCTEXT_NAMESPACE "PCGExSampleSurfaceGuidedElement"

PCGExData::EInit UPCGExSampleSurfaceGuidedSettings::GetPointOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

int32 UPCGExSampleSurfaceGuidedSettings::GetPreferredChunkSize() const { return 32; }

FPCGElementPtr UPCGExSampleSurfaceGuidedSettings::CreateElement() const { return MakeShared<FPCGExSampleSurfaceGuidedElement>(); }

FPCGExSampleSurfaceGuidedContext::~FPCGExSampleSurfaceGuidedContext()
{
	PCGEX_CLEANUP_ASYNC

	PCGEX_SAMPLENEARESTTRACE_FOREACH(PCGEX_OUTPUT_DELETE)
}

FPCGContext* FPCGExSampleSurfaceGuidedElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExSampleSurfaceGuidedContext* Context = new FPCGExSampleSurfaceGuidedContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	PCGEX_SETTINGS(UPCGExSampleSurfaceGuidedSettings)

	PCGEX_FWD(CollisionChannel)
	PCGEX_FWD(CollisionObjectType)
	PCGEX_FWD(ProfileName)
	PCGEX_FWD(bIgnoreSelf)

	PCGEX_FWD(Size)
	PCGEX_FWD(bUseLocalSize)
	PCGEX_FWD(bProjectFailToSize)

	Context->SizeGetter.Capture(Settings->LocalSize);

	Context->DirectionGetter.Capture(Settings->Direction);

	PCGEX_SAMPLENEARESTTRACE_FOREACH(PCGEX_OUTPUT_FWD)

	return Context;
}

bool FPCGExSampleSurfaceGuidedElement::Validate(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Validate(InContext)) { return false; }

	PCGEX_CONTEXT(FPCGExSampleSurfaceGuidedContext)
	PCGEX_SETTINGS(UPCGExSampleSurfaceGuidedSettings)

	PCGEX_SAMPLENEARESTTRACE_FOREACH(PCGEX_OUTPUT_VALIDATE_NAME)

	return true;
}

bool FPCGExSampleSurfaceGuidedElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleSurfaceGuidedElement::Execute);

	PCGEX_CONTEXT(FPCGExSampleSurfaceGuidedContext)

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
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
	PCGEX_ASYNC_CHECKPOINT

	const FVector Origin = PointIO->GetInPoint(TaskInfos.Index).Transform.GetLocation();

	FCollisionQueryParams CollisionParams;
	CollisionParams.bTraceComplex = true;
	CollisionParams.AddIgnoredActors(Context->IgnoredActors);

	const double Size = Context->bUseLocalSize ? Context->SizeGetter[TaskInfos.Index] : Context->Size;
	const FVector Trace = Context->DirectionGetter[TaskInfos.Index] * Size;
	const FVector End = Origin + Trace;

	bool bSuccess = false;
	FHitResult HitResult;

	auto ProcessTraceResult = [&]()
	{
		PCGEX_OUTPUT_VALUE(Location, TaskInfos.Index, HitResult.ImpactPoint)
		PCGEX_OUTPUT_VALUE(Normal, TaskInfos.Index, HitResult.Normal)
		PCGEX_OUTPUT_VALUE(Distance, TaskInfos.Index, FVector::Distance(HitResult.ImpactPoint, Origin))
		bSuccess = true;
	};

	PCGEX_ASYNC_CHECKPOINT

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
		if (Context->World->LineTraceSingleByProfile(HitResult, Origin, End, Context->ProfileName, CollisionParams))
		{
			ProcessTraceResult();
		}
		break;
	default: ;
	}

	PCGEX_ASYNC_CHECKPOINT
	if (Context->bProjectFailToSize)
	{
		PCGEX_OUTPUT_VALUE(Location, TaskInfos.Index, End)
		PCGEX_OUTPUT_VALUE(Normal, TaskInfos.Index, Trace.GetSafeNormal()*-1)
		PCGEX_OUTPUT_VALUE(Distance, TaskInfos.Index, Size)
	}

	PCGEX_OUTPUT_VALUE(Success, TaskInfos.Index, bSuccess)
	return bSuccess;
}

#undef LOCTEXT_NAMESPACE
