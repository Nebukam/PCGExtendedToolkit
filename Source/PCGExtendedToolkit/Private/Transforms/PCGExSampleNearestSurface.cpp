// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transforms/PCGExSampleNearestSurface.h"
#include "PCGExCommon.h"

#define LOCTEXT_NAMESPACE "PCGExSampleNearestSurfaceElement"

PCGEx::EIOInit UPCGExSampleNearestSurfaceSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::DuplicateInput; }

FPCGElementPtr UPCGExSampleNearestSurfaceSettings::CreateElement() const { return MakeShared<FPCGExSampleNearestSurfaceElement>(); }

void FPCGExSampleNearestSurfaceContext::ProcessSweepHit(const PCGExAsync::FSweepSphereTask* Task)
{
	WrapSweepTask(Task, true);
}

void FPCGExSampleNearestSurfaceContext::ProcessSweepMiss(const PCGExAsync::FSweepSphereTask* Task)
{
	if (Task->Infos.Attempt > NumMaxAttempts)
	{
		WrapSweepTask(Task, false);
		return;
	}

	ScheduleTask<PCGExAsync::FSweepSphereTask>(Task->Infos.GetRetry());
}

void FPCGExSampleNearestSurfaceContext::WrapSweepTask(const PCGExAsync::FSweepSphereTask* Task, bool bSuccess)
{
	FWriteScopeLock ScopeLock(ContextLock);
	NumSweepComplete++;
}

FPCGContext* FPCGExSampleNearestSurfaceElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExSampleNearestSurfaceContext* Context = new FPCGExSampleNearestSurfaceContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExSampleNearestSurfaceSettings* Settings = Context->GetInputSettings<UPCGExSampleNearestSurfaceSettings>();
	check(Settings);

	Context->AttemptStepSize = FMath::Max(Settings->MaxDistance / static_cast<double>(Settings->NumMaxAttempts), Settings->MinStepSize);
	Context->NumMaxAttempts = FMath::Max(static_cast<int32>(static_cast<double>(Settings->MaxDistance) / Context->AttemptStepSize), 1);

	Context->CollisionType = Settings->CollisionType;
	Context->CollisionChannel = Settings->CollisionChannel;
	Context->CollisionObjectType = Settings->CollisionObjectType;

	Context->bIgnoreSelf = Settings->bIgnoreSelf;

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

	auto InitializeForIO = [&Context](UPCGExPointIO* IO)
	{
		Context->NumSweepComplete = 0;
		IO->BuildMetadataEntries();
		PCGEX_INIT_ATTRIBUTE_OUT(Location, FVector)
		PCGEX_INIT_ATTRIBUTE_OUT(LookAt, FVector)
		PCGEX_INIT_ATTRIBUTE_OUT(Normal, FVector)
		PCGEX_INIT_ATTRIBUTE_OUT(Distance, double)
	};

	auto ProcessPoint = [&Context, this](const FPCGPoint& Point, const int32 Index, const UPCGExPointIO* IO)
	{
		Context->ScheduleTask<PCGExAsync::FSweepSphereTask>(Index, Point.MetadataEntry);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints))
	{
		if (Context->CurrentIO->OutputParallelProcessing(Context, InitializeForIO, ProcessPoint, Context->ChunkSize))
		{
			Context->SetState(PCGExMT::EState::WaitingOnAsyncTasks);
		}
	}

	if (Context->IsState(PCGExMT::EState::WaitingOnAsyncTasks))
	{
		if (Context->NumSweepComplete == Context->CurrentIO->NumPoints)
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

#undef LOCTEXT_NAMESPACE
