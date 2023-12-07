// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestSurface.h"

#define LOCTEXT_NAMESPACE "PCGExSampleNearestSurfaceElement"


PCGExPointIO::EInit UPCGExSampleNearestSurfaceSettings::GetPointOutputInitMode() const { return PCGExPointIO::EInit::DuplicateInput; }

int32 UPCGExSampleNearestSurfaceSettings::GetPreferredChunkSize() const { return 32; }

FPCGElementPtr UPCGExSampleNearestSurfaceSettings::CreateElement() const { return MakeShared<FPCGExSampleNearestSurfaceElement>(); }

FPCGContext* FPCGExSampleNearestSurfaceElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExSampleNearestSurfaceContext* Context = new FPCGExSampleNearestSurfaceContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExSampleNearestSurfaceSettings* Settings = Context->GetInputSettings<UPCGExSampleNearestSurfaceSettings>();
	check(Settings);

	Context->RangeMax = Settings->MaxDistance;

	Context->CollisionType = Settings->CollisionType;
	Context->CollisionChannel = Settings->CollisionChannel;
	Context->CollisionObjectType = Settings->CollisionObjectType;
	Context->ProfileName = Settings->ProfileName;

	Context->bIgnoreSelf = Settings->bIgnoreSelf;

	PCGEX_FORWARD_OUT_ATTRIBUTE(Success)
	PCGEX_FORWARD_OUT_ATTRIBUTE(Location)
	PCGEX_FORWARD_OUT_ATTRIBUTE(LookAt)
	PCGEX_FORWARD_OUT_ATTRIBUTE(Normal)
	PCGEX_FORWARD_OUT_ATTRIBUTE(Distance)

	return Context;
}

bool FPCGExSampleNearestSurfaceElement::Validate(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Validate(InContext)) { return false; }

	FPCGExSampleNearestSurfaceContext* Context = static_cast<FPCGExSampleNearestSurfaceContext*>(InContext);
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Success)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Location)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(LookAt)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Normal)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Distance)
	return true;
}

bool FPCGExSampleNearestSurfaceElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestSurfaceElement::Execute);

	FPCGExSampleNearestSurfaceContext* Context = static_cast<FPCGExSampleNearestSurfaceContext*>(InContext);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
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
		auto Initialize = [&](UPCGExPointIO* PointIO) //UPCGExPointIO* PointIO
		{
			PointIO->BuildMetadataEntries();
			PCGEX_INIT_ATTRIBUTE_OUT(Success, bool)
			PCGEX_INIT_ATTRIBUTE_OUT(Location, FVector)
			PCGEX_INIT_ATTRIBUTE_OUT(LookAt, FVector)
			PCGEX_INIT_ATTRIBUTE_OUT(Normal, FVector)
			PCGEX_INIT_ATTRIBUTE_OUT(Distance, double)
		};

		auto ProcessPoint = [&](int32 PointIndex, const UPCGExPointIO* PointIO)
		{
			Context->GetAsyncManager()->StartTask<FSweepSphereTask>(PointIndex, PointIO->GetOutPoint(PointIndex).MetadataEntry, Context->CurrentIO);
		};

		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint)) { Context->StartAsyncWait(PCGExMT::State_WaitingOnAsyncWork); }
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (Context->IsAsyncWorkComplete()) { Context->StopAsyncWait(PCGExMT::State_ReadyForNextPoints); }
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
		return true;
	}

	return false;
}

bool FSweepSphereTask::ExecuteTask()
{
	
	const FPCGExSampleNearestSurfaceContext* Context = Manager->GetContext<FPCGExSampleNearestSurfaceContext>();
	PCGEX_ASYNC_LIFE_CHECK
	
	const FPCGPoint& InPoint = PointIO->GetInPoint(TaskInfos.Index);
	const FVector Origin = InPoint.Transform.GetLocation();

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
			PCGEX_ASYNC_LIFE_CHECK_RET
			const FVector Direction = (HitLocation - Origin).GetSafeNormal();
			PCGEX_SET_OUT_ATTRIBUTE(Location, TaskInfos.Key, HitLocation)
			PCGEX_SET_OUT_ATTRIBUTE(Normal, TaskInfos.Key, Direction*-1) // TODO: expose "precise normal" in which case we line trace to location
			PCGEX_SET_OUT_ATTRIBUTE(LookAt, TaskInfos.Key, Direction)
			PCGEX_SET_OUT_ATTRIBUTE(Distance, TaskInfos.Key, MinDist)
		}
	};

	PCGEX_ASYNC_LIFE_CHECK

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
		if (Context->World->OverlapMultiByProfile(OutOverlaps, Origin, FQuat::Identity, Context->ProfileName, CollisionShape, CollisionParams))
		{
			ProcessOverlapResults();
		}
		break;
	default: ;
	}

	PCGEX_ASYNC_LIFE_CHECK
	PCGEX_SET_OUT_ATTRIBUTE(Success, TaskInfos.Key, bSuccess)
	return bSuccess;
}

#undef LOCTEXT_NAMESPACE
