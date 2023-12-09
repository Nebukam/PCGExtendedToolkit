// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleSurfaceGuided.h"
#include "Elements/PCGActorSelector.h"

#define LOCTEXT_NAMESPACE "PCGExSampleSurfaceGuidedElement"

PCGExPointIO::EInit UPCGExSampleSurfaceGuidedSettings::GetPointOutputInitMode() const { return PCGExPointIO::EInit::DuplicateInput; }

int32 UPCGExSampleSurfaceGuidedSettings::GetPreferredChunkSize() const { return 32; }

FPCGElementPtr UPCGExSampleSurfaceGuidedSettings::CreateElement() const { return MakeShared<FPCGExSampleSurfaceGuidedElement>(); }

FPCGContext* FPCGExSampleSurfaceGuidedElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExSampleSurfaceGuidedContext* Context = new FPCGExSampleSurfaceGuidedContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExSampleSurfaceGuidedSettings* Settings = Context->GetInputSettings<UPCGExSampleSurfaceGuidedSettings>();
	check(Settings);

	Context->CollisionChannel = Settings->CollisionChannel;
	Context->CollisionObjectType = Settings->CollisionObjectType;
	Context->ProfileName = Settings->ProfileName;

	Context->bIgnoreSelf = Settings->bIgnoreSelf;

	Context->Size = Settings->Size;
	Context->bUseLocalSize = Settings->bUseLocalSize;
	Context->SizeGetter.Capture(Settings->LocalSize);
	Context->bProjectFailToSize = Context->bProjectFailToSize;

	Context->DirectionGetter.Capture(Settings->Direction);
	PCGEX_FORWARD_OUT_ATTRIBUTE(Success)
	PCGEX_FORWARD_OUT_ATTRIBUTE(Location)
	PCGEX_FORWARD_OUT_ATTRIBUTE(Normal)
	PCGEX_FORWARD_OUT_ATTRIBUTE(Distance)

	return Context;
}

bool FPCGExSampleSurfaceGuidedElement::Validate(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Validate(InContext)) { return false; }

	FPCGExSampleSurfaceGuidedContext* Context = static_cast<FPCGExSampleSurfaceGuidedContext*>(InContext);
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Success)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Location)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Normal)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Distance)
	return true;
}

bool FPCGExSampleSurfaceGuidedElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleSurfaceGuidedElement::Execute);

	FPCGExSampleSurfaceGuidedContext* Context = static_cast<FPCGExSampleSurfaceGuidedContext*>(InContext);

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
		auto Initialize = [&](FPCGExPointIO& PointIO) //UPCGExPointIO* PointIO
		{
			Context->DirectionGetter.Validate(PointIO.GetOut());
			PointIO.BuildMetadataEntries();

			PCGEX_INIT_ATTRIBUTE_OUT(Success, bool)
			PCGEX_INIT_ATTRIBUTE_OUT(Location, FVector)
			PCGEX_INIT_ATTRIBUTE_OUT(Normal, FVector)
			PCGEX_INIT_ATTRIBUTE_OUT(Distance, double)
		};

		auto ProcessPoint = [&](const int32 PointIndex, const FPCGExPointIO& PointIO)
		{
			Context->GetAsyncManager()->StartTask<FTraceTask>(PointIndex, PointIO.GetOutPoint(PointIndex).MetadataEntry, Context->CurrentIO);
		};

		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint)) { Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork); }
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (Context->IsAsyncWorkComplete()) { Context->SetState(PCGExMT::State_ReadyForNextPoints); }
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
		return true;
	}

	return false;
}

bool FTraceTask::ExecuteTask()
{
	const FPCGExSampleSurfaceGuidedContext* Context = Manager->GetContext<FPCGExSampleSurfaceGuidedContext>();
	PCGEX_ASYNC_LIFE_CHECK

	const FPCGPoint& InPoint = PointIO->GetInPoint(TaskInfos.Index);
	const FVector Origin = InPoint.Transform.GetLocation();

	FCollisionQueryParams CollisionParams;
	CollisionParams.bTraceComplex = true;
	CollisionParams.AddIgnoredActors(Context->IgnoredActors);

	const double Size = Context->bUseLocalSize ? Context->SizeGetter.GetValue(InPoint) : Context->Size;
	const FVector Trace = Context->DirectionGetter.GetValue(InPoint) * Size;
	const FVector End = Origin + Trace;

	bool bSuccess = false;
	FHitResult HitResult;

	auto ProcessTraceResult = [&]()
	{
		PCGEX_SET_OUT_ATTRIBUTE(Location, TaskInfos.Key, HitResult.ImpactPoint)
		PCGEX_SET_OUT_ATTRIBUTE(Normal, TaskInfos.Key, HitResult.Normal)
		PCGEX_SET_OUT_ATTRIBUTE(Distance, TaskInfos.Key, FVector::Distance(HitResult.ImpactPoint, Origin))
		bSuccess = true;
	};

	PCGEX_ASYNC_LIFE_CHECK

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

	PCGEX_ASYNC_LIFE_CHECK
	if (Context->bProjectFailToSize)
	{
		PCGEX_SET_OUT_ATTRIBUTE(Location, TaskInfos.Key, End)
		PCGEX_SET_OUT_ATTRIBUTE(Normal, TaskInfos.Key, Trace.GetSafeNormal()*-1)
		PCGEX_SET_OUT_ATTRIBUTE(Distance, TaskInfos.Key, Size)
	}

	PCGEX_SET_OUT_ATTRIBUTE(Success, TaskInfos.Key, bSuccess)
	return bSuccess;
}

#undef LOCTEXT_NAMESPACE
