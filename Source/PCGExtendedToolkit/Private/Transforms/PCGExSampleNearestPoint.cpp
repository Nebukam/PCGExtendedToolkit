// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "..\..\Public\Transforms\PCGExSampleNearestPoint.h"
#include "Data/PCGSpatialData.h"
#include "PCGContext.h"
#include "PCGExCommon.h"

#define LOCTEXT_NAMESPACE "PCGExSampleNearestPointElement"

PCGEx::EIOInit UPCGExSampleNearestPointSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::DuplicateInput; }

FPCGElementPtr UPCGExSampleNearestPointSettings::CreateElement() const { return MakeShared<FPCGExSampleNearestPointElement>(); }

void FPCGExSampleNearestPointContext::ProcessSweepHit(const PCGExAsync::FSweepSphereTask* Task)
{
	WrapSweepTask(Task, true);
}

void FPCGExSampleNearestPointContext::ProcessSweepMiss(const PCGExAsync::FSweepSphereTask* Task)
{
	if (Task->Infos.Attempt > NumMaxAttempts)
	{
		WrapSweepTask(Task, false);
		return;
	}

	ScheduleTask<PCGExAsync::FSweepSphereTask>(Task->Infos.GetRetry());
}

void FPCGExSampleNearestPointContext::WrapSweepTask(const PCGExAsync::FSweepSphereTask* Task, bool bSuccess)
{
	FWriteScopeLock ScopeLock(ContextLock);
	NumSweepComplete++;
}

FPCGContext* FPCGExSampleNearestPointElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExSampleNearestPointContext* Context = new FPCGExSampleNearestPointContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExSampleNearestPointSettings* Settings = Context->GetInputSettings<UPCGExSampleNearestPointSettings>();
	check(Settings);

	Context->AttemptStepSize = FMath::Max(Settings->MaxDistance / static_cast<double>(Settings->NumMaxAttempts), Settings->MinStepSize);
	Context->NumMaxAttempts = FMath::Max(static_cast<int32>(static_cast<double>(Settings->MaxDistance) / Context->AttemptStepSize), 1);
	Context->CollisionChannel = Settings->CollisionChannel;
	Context->bIgnoreSelf = Settings->bIgnoreSelf;

	PCGEX_FORWARD_ATTRIBUTE(Location, bWriteLocation, Location)
	PCGEX_FORWARD_ATTRIBUTE(Direction, bWriteDirection, Direction)
	PCGEX_FORWARD_ATTRIBUTE(Normal, bWriteNormal, Normal)
	PCGEX_FORWARD_ATTRIBUTE(Distance, bWriteDistance, Distance)

	return Context;
}

bool FPCGExSampleNearestPointElement::Validate(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Validate(InContext)) { return false; }

	FPCGExSampleNearestPointContext* Context = static_cast<FPCGExSampleNearestPointContext*>(InContext);
	PCGEX_CHECK_OUTNAME(Location)
	PCGEX_CHECK_OUTNAME(Direction)
	PCGEX_CHECK_OUTNAME(Normal)
	PCGEX_CHECK_OUTNAME(Distance)
	return true;
}

bool FPCGExSampleNearestPointElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestPointElement::Execute);

	FPCGExSampleNearestPointContext* Context = static_cast<FPCGExSampleNearestPointContext*>(InContext);

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
		PCGEX_INIT_ATTRIBUTE_OUT(Direction, FVector)
		PCGEX_INIT_ATTRIBUTE_OUT(Normal, FVector)
		PCGEX_INIT_ATTRIBUTE_OUT(Distance, double)
	};

	auto ProcessPoint = [&Context, this](const FPCGPoint& Point, const int32 Index, UPCGExPointIO* IO)
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
