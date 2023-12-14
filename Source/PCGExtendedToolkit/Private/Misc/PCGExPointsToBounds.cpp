// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExPointsToBounds.h"

#define LOCTEXT_NAMESPACE "PCGExPointsToBoundsElement"
#define PCGEX_NAMESPACE PointsToBounds

PCGExData::EInit UPCGExPointsToBoundsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FPCGElementPtr UPCGExPointsToBoundsSettings::CreateElement() const { return MakeShared<FPCGExPointsToBoundsElement>(); }

FPCGExPointsToBoundsContext::~FPCGExPointsToBoundsContext()
{
	PCGEX_TERMINATE_ASYNC
}

PCGEX_INITIALIZE_CONTEXT(PointsToBounds)

bool FPCGExPointsToBoundsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PointsToBounds)

	//PCGEX_FWD(bOutputNormalizedIndex)
	//PCGEX_FWD(OutputAttributeName)

	//PCGEX_VALIDATE_NAME(Context->OutputAttributeName)

	return true;
}

bool FPCGExPointsToBoundsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPointsToBoundsElement::Execute);

	PCGEX_CONTEXT(PointsToBounds)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else { Context->SetState(PCGExMT::State_ProcessingPoints); }
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
