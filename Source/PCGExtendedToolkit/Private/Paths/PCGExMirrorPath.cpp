// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExMirrorPath.h"

#define LOCTEXT_NAMESPACE "PCGExMirrorPathElement"
#define PCGEX_NAMESPACE MirrorPath

PCGExData::EInit UPCGExMirrorPathSettings::GetMainOutputInitMode() const { return bKeepOriginalPaths ? PCGExData::EInit::Forward : PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(MirrorPath)

FPCGExMirrorPathContext::~FPCGExMirrorPathContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExMirrorPathElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(MirrorPath)

	return true;
}

bool FPCGExMirrorPathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExMirrorPathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(MirrorPath)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			Context->Done();
			return false;
		}
	}

	if (Context->IsDone())
	{
		Context->OutputMainPoints();
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}

bool FPCGExMirrorPathTask::ExecuteTask()
{
	const FPCGExMirrorPathContext* Context = Manager->GetContext<FPCGExMirrorPathContext>();
	PCGEX_SETTINGS(MirrorPath)

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
