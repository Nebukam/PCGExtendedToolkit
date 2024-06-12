// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExShrinkPath.h"

#define LOCTEXT_NAMESPACE "PCGExShrinkPathElement"
#define PCGEX_NAMESPACE ShrinkPath

PCGExData::EInit UPCGExShrinkPathSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

PCGEX_INITIALIZE_ELEMENT(ShrinkPath)

FPCGExShrinkPathContext::~FPCGExShrinkPathContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExShrinkPathElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ShrinkPath)

	return true;
}

bool FPCGExShrinkPathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExShrinkPathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ShrinkPath)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		while (Context->AdvancePointsIO())
		{
			Context->GetAsyncManager()->Start<FPCGExShrinkPathTask>(Context->CurrentIO->IOIndex, Context->CurrentIO);
		}

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC

		Context->Done();
	}

	if (Context->IsDone())
	{
		Context->OutputMainPoints();
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}

bool FPCGExShrinkPathTask::ExecuteTask()
{
	const FPCGExShrinkPathContext* Context = Manager->GetContext<FPCGExShrinkPathContext>();
	PCGEX_SETTINGS(ShrinkPath)

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
