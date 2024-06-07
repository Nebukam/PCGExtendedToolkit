// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "..\..\Public\Graph\PCGExCopyClustersToPoints.h"

#include "Data/PCGExGraphDefinition.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExCopyClustersToPointsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::Forward; }
PCGExData::EInit UPCGExCopyClustersToPointsSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::Forward; }

#pragma endregion

FPCGExCopyClustersToPointsContext::~FPCGExCopyClustersToPointsContext()
{
	PCGEX_TERMINATE_ASYNC
}

PCGEX_INITIALIZE_ELEMENT(CopyClustersToPoints)

bool FPCGExCopyClustersToPointsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(CopyClustersToPoints)

	return true;
}

bool FPCGExCopyClustersToPointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCopyClustersToPointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(CopyClustersToPoints)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	while (Context->AdvancePointsIO())
	{
		FString OutId;
		Context->CurrentIO->Tags->Set(PCGExGraph::TagStr_ClusterPair, Context->CurrentIO->GetOut()->UID, OutId);

		if (!Context->TaggedEdges) { continue; }

		for (const PCGExData::FPointIO* Entry : Context->TaggedEdges->Entries) { Entry->Tags->Set(PCGExGraph::TagStr_ClusterPair, OutId); }
	}

	Context->OutputPointsAndEdges();
	Context->Done();
	Context->ExecutionComplete();

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
