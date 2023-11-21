// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transforms/PCGExSampleSurfaceGuided.h"
#include "Data/PCGSpatialData.h"
#include "PCGContext.h"
#include "PCGExCommon.h"
#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExSampleSurfaceGuidedElement"

PCGEx::EIOInit UPCGExSampleSurfaceGuidedSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::DuplicateInput; }

FPCGElementPtr UPCGExSampleSurfaceGuidedSettings::CreateElement() const { return MakeShared<FPCGExSampleSurfaceGuidedElement>(); }

void FPCGExSampleSurfaceGuidedContext::WrapTraceTask(const PCGExAsync::FTraceTask* Task, bool bSuccess)
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
	
	PCGEX_FORWARD_OUT_ATTRIBUTE(SurfaceLocation)
	PCGEX_FORWARD_OUT_ATTRIBUTE(SurfaceNormal)
	PCGEX_FORWARD_OUT_ATTRIBUTE(Distance)

	return Context;
}

bool FPCGExSampleSurfaceGuidedElement::Validate(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Validate(InContext)) { return false; }

	FPCGExSampleSurfaceGuidedContext* Context = static_cast<FPCGExSampleSurfaceGuidedContext*>(InContext);
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(SurfaceLocation)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(SurfaceNormal)
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

	auto InitializeForIO = [&Context](UPCGExPointIO* IO)
	{
		Context->NumTraceComplete = 0;
		Context->Direction.Validate(IO->Out);
		IO->BuildMetadataEntries();
		PCGEX_INIT_ATTRIBUTE_OUT(SurfaceLocation, FVector)
		PCGEX_INIT_ATTRIBUTE_OUT(SurfaceNormal, FVector)
		PCGEX_INIT_ATTRIBUTE_OUT(Distance, double)
	};

	auto ProcessPoint = [&Context, this](const FPCGPoint& Point, const int32 Index, UPCGExPointIO* IO)
	{
		Context->ScheduleTask<PCGExAsync::FTraceTask>(Index, Point.MetadataEntry);
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
