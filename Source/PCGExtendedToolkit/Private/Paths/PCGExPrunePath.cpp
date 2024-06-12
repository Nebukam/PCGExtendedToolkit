// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPrunePath.h"

#define LOCTEXT_NAMESPACE "PCGExPrunePathElement"
#define PCGEX_NAMESPACE PrunePath

TArray<FPCGPinProperties> UPCGExPrunePathSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGEx::SourceTargetsLabel, "Intersection targets", Required, {})
	return PinProperties;
}

PCGExData::EInit UPCGExPrunePathSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

PCGEX_INITIALIZE_ELEMENT(PrunePath)

FPCGExPrunePathContext::~FPCGExPrunePathContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExPrunePathElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PrunePath)

	return true;
}

bool FPCGExPrunePathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPrunePathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PrunePath)

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

bool FPCGExPrunePathTask::ExecuteTask()
{
	const FPCGExPrunePathContext* Context = Manager->GetContext<FPCGExPrunePathContext>();
	PCGEX_SETTINGS(PrunePath)

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
