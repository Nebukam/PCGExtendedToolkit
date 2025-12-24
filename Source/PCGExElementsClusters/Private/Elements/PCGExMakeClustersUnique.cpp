// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExMakeClustersUnique.h"

#include "Clusters/PCGExClustersHelpers.h"
#include "Data/PCGExPointIO.h"


#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EIOInit UPCGExMakeClustersUniqueSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Forward; }
PCGExData::EIOInit UPCGExMakeClustersUniqueSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Forward; }

#pragma endregion

PCGEX_INITIALIZE_ELEMENT(MakeClustersUnique)

bool FPCGExMakeClustersUniqueElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExClustersProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(MakeClustersUnique)

	return true;
}

bool FPCGExMakeClustersUniqueElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExMakeClustersUniqueElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(MakeClustersUnique)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		Context->SetState(PCGExCommon::States::State_ReadyForNextPoints);
	}

	while (Context->AdvancePointsIO(false))
	{
		PCGExDataId OutId;
		PCGExClusters::Helpers::SetClusterVtx(Context->CurrentIO, OutId);

		if (!Context->TaggedEdges) { continue; }

		PCGExClusters::Helpers::MarkClusterEdges(Context->TaggedEdges->Entries, OutId);
	}

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
