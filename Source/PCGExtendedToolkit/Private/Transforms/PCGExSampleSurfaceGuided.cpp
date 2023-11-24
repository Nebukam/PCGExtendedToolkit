// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transforms/PCGExSampleSurfaceGuided.h"

#define LOCTEXT_NAMESPACE "PCGExSampleSurfaceGuidedElement"

PCGExIO::EInitMode UPCGExSampleSurfaceGuidedSettings::GetPointOutputInitMode() const { return PCGExIO::EInitMode::DuplicateInput; }

FPCGElementPtr UPCGExSampleSurfaceGuidedSettings::CreateElement() const { return MakeShared<FPCGExSampleSurfaceGuidedElement>(); }

void FPCGExSampleSurfaceGuidedContext::WrapTraceTask(const FPointTask* Task, bool bSuccess)
{
	FWriteScopeLock ScopeLock(ContextLock);
	NumTraceComplete++;
}

FPCGContext* FPCGExSampleSurfaceGuidedElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExSampleSurfaceGuidedContext* Context = new FPCGExSampleSurfaceGuidedContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExSampleSurfaceGuidedSettings* Settings = Context->GetInputSettings<UPCGExSampleSurfaceGuidedSettings>();
	check(Settings);

	Context->CollisionChannel = Settings->CollisionChannel;
	Context->CollisionObjectType = Settings->CollisionObjectType;

	Context->bIgnoreSelf = Settings->bIgnoreSelf;

	Context->Size = Settings->Size;
	Context->bUseLocalSize = Settings->bUseLocalSize;
	Context->LocalSize.Capture(Settings->LocalSize);

	Context->Direction.Capture(Settings->Direction);

	PCGEX_FORWARD_OUT_ATTRIBUTE(Location)
	PCGEX_FORWARD_OUT_ATTRIBUTE(Normal)
	PCGEX_FORWARD_OUT_ATTRIBUTE(Distance)

	return Context;
}

bool FPCGExSampleSurfaceGuidedElement::Validate(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Validate(InContext)) { return false; }

	FPCGExSampleSurfaceGuidedContext* Context = static_cast<FPCGExSampleSurfaceGuidedContext*>(InContext);
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Location)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Normal)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Distance)
	return true;
}

bool FPCGExSampleSurfaceGuidedElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleSurfaceGuidedElement::Execute);

	FPCGExSampleSurfaceGuidedContext* Context = static_cast<FPCGExSampleSurfaceGuidedContext*>(InContext);

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

	auto Initialize = [&](UPCGExPointIO* PointIO)
	{
		UPCGExPointIO* PointIO = Context->CurrentIO;
		Context->NumTraceComplete = 0;
		Context->Direction.Validate(PointIO->Out);
		PointIO->BuildMetadataEntries();

		PCGEX_INIT_ATTRIBUTE_OUT(Location, FVector)
		PCGEX_INIT_ATTRIBUTE_OUT(Normal, FVector)
		PCGEX_INIT_ATTRIBUTE_OUT(Distance, double)
	};

	auto ProcessPoint = [&](const FPCGPoint& Point, const int32 Index, UPCGExPointIO* PointIO)
	{
		Context->ScheduleTask<FTraceTask>(Index, Point.MetadataEntry);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints))
	{
		/*
		if (PCGExMT::ParallelForLoop(Context, Context->CurrentIO->NumPoints, Initialize, [&](int32 Index) { ProcessPoint(Context->CurrentIO->Out->GetPoint(Index), Index, Context->CurrentIO); }, Context->ChunkSize))
		{
			Context->SetState(PCGExMT::EState::WaitingOnAsyncTasks);
			Context->World->AsyncOverlapByProfile()
		}
		*/
		if (Context->CurrentIO->OutputParallelProcessing(Context, Initialize, ProcessPoint, Context->ChunkSize))
		{
			Context->SetState(PCGExMT::EState::WaitingOnAsyncTasks);
		}
	}

	if (Context->IsState(PCGExMT::EState::WaitingOnAsyncTasks))
	{
		if (Context->NumTraceComplete == Context->CurrentIO->NumPoints)
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
