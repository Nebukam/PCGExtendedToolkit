// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestSurface.h"

#define LOCTEXT_NAMESPACE "PCGExSampleNearestSurfaceElement"

PCGExIO::EInitMode UPCGExSampleNearestSurfaceSettings::GetPointOutputInitMode() const { return PCGExIO::EInitMode::DuplicateInput; }

int32 UPCGExSampleNearestSurfaceSettings::GetPreferredChunkSize() const { return 32; }

FPCGElementPtr UPCGExSampleNearestSurfaceSettings::CreateElement() const { return MakeShared<FPCGExSampleNearestSurfaceElement>(); }

FPCGContext* FPCGExSampleNearestSurfaceElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExSampleNearestSurfaceContext* Context = new FPCGExSampleNearestSurfaceContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExSampleNearestSurfaceSettings* Settings = Context->GetInputSettings<UPCGExSampleNearestSurfaceSettings>();
	check(Settings);

	Context->AttemptStepSize = FMath::Max(Settings->MaxDistance / static_cast<double>(Settings->NumMaxAttempts), Settings->MinStepSize);
	Context->NumMaxAttempts = FMath::Max(static_cast<int32>(static_cast<double>(Settings->MaxDistance) / Context->AttemptStepSize), 1);
	Context->RangeMax = Settings->MaxDistance;

	Context->CollisionType = Settings->CollisionType;
	Context->CollisionChannel = Settings->CollisionChannel;
	Context->CollisionObjectType = Settings->CollisionObjectType;

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

	if (Context->IsState(PCGExMT::EState::Setup))
	{
		if (!Validate(Context)) { return true; }
		Context->SetState(PCGExMT::EState::ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::EState::ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			Context->SetState(PCGExMT::EState::Done);
		}
		else
		{
			Context->SetState(PCGExMT::EState::ProcessingPoints);
		}
	}

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
		FAsyncTask<FSweepSphereTask>* Task = Context->CreateTask<FSweepSphereTask>(PointIndex, PointIO->GetOutPoint(PointIndex).MetadataEntry);
		Task->GetTask().RangeMax = Context->RangeMax; //TODO: Localize range
		Context->StartTask(Task);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints))
	{
		if (Context->AsyncProcessingCurrentPoints(Initialize, ProcessPoint))
		{
			Context->SetState(PCGExMT::EState::WaitingOnAsyncWork);
		}
	}

	if (Context->IsState(PCGExMT::EState::WaitingOnAsyncWork))
	{
		if (Context->IsAsyncWorkComplete())
		{
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
		}
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
		return true;
	}

	return false;
}

void FSweepSphereTask::ExecuteTask()
{
	const FPCGExSampleNearestSurfaceContext* Context = static_cast<FPCGExSampleNearestSurfaceContext*>(TaskContext);
	const FPCGPoint& InPoint = PointData->GetInPoint(Infos.Index);
	const FVector Origin = InPoint.Transform.GetLocation();

	FCollisionQueryParams CollisionParams;
	if (Context->bIgnoreSelf) { CollisionParams.AddIgnoredActor(TaskContext->SourceComponent->GetOwner()); }

	const FCollisionShape CollisionShape = FCollisionShape::MakeSphere(RangeMax);

	if (!IsTaskValid()) { return; }

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
			PCGEX_SET_OUT_ATTRIBUTE(Location, Infos.Key, HitLocation)
			PCGEX_SET_OUT_ATTRIBUTE(Normal, Infos.Key, Direction*-1) // TODO: expose "precise normal" in which case we line trace to location
			PCGEX_SET_OUT_ATTRIBUTE(LookAt, Infos.Key, Direction)
			PCGEX_SET_OUT_ATTRIBUTE(Distance, Infos.Key, MinDist)
		}
	};


	if (Context->CollisionType == EPCGExCollisionFilterType::Channel)
	{
		if (Context->World->OverlapMultiByChannel(OutOverlaps, Origin, FQuat::Identity, Context->CollisionChannel, CollisionShape, CollisionParams))
		{
			ProcessOverlapResults();
		}
	}
	else
	{
		if (FCollisionObjectQueryParams ObjectQueryParams = FCollisionObjectQueryParams(Context->CollisionObjectType);
			Context->World->OverlapMultiByObjectType(OutOverlaps, Origin, FQuat::Identity, ObjectQueryParams, CollisionShape, CollisionParams))
		{
			ProcessOverlapResults();
		}
	}

	PCGEX_SET_OUT_ATTRIBUTE(Success, Infos.Key, bSuccess)
	ExecutionComplete(bSuccess);
}

#undef LOCTEXT_NAMESPACE
