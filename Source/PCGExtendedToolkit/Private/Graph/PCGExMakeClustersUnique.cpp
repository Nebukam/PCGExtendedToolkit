// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExMakeClustersUnique.h"


#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EIOInit UPCGExMakeClustersUniqueSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Forward; }
PCGExData::EIOInit UPCGExMakeClustersUniqueSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Forward; }

#pragma endregion

PCGEX_INITIALIZE_ELEMENT(MakeClustersUnique)

bool FPCGExMakeClustersUniqueElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(MakeClustersUnique)

	return true;
}

bool FPCGExMakeClustersUniqueElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExMakeClustersUniqueElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(MakeClustersUnique)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		Context->SetState(PCGEx::State_ReadyForNextPoints);
	}

	while (Context->AdvancePointsIO(false))
	{
		FString OutId;
		PCGExGraph::SetClusterVtx(Context->CurrentIO, OutId);

		if (!Context->TaggedEdges) { continue; }

		PCGExGraph::MarkClusterEdges(Context->TaggedEdges->Entries, OutId);
	}

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
