// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBoundsClustersIntersection.h"


#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExBoundsClustersIntersectionSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }
PCGExData::EInit UPCGExBoundsClustersIntersectionSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

#pragma endregion

FPCGExBoundsClustersIntersectionContext::~FPCGExBoundsClustersIntersectionContext()
{
	PCGEX_TERMINATE_ASYNC

	IndexedEdges.Empty();
}

PCGEX_INITIALIZE_ELEMENT(BoundsClustersIntersection)

bool FPCGExBoundsClustersIntersectionElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BoundsClustersIntersection)

	return true;
}

bool FPCGExBoundsClustersIntersectionElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBoundsClustersIntersectionElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BoundsClustersIntersection)

	PCGE_LOG(Error, GraphAndLog, FTEXT("NOT IMPLEMENTED YET"));
	return true;

	/*
	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges) { return false; }

			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		while (Context->AdvanceEdges(false))
		{
		}

		Context->SetAsyncState(PCGExGraph::State_WritingClusters);
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		PCGEX_ASYNC_WAIT

		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->OutputMainPoints();
	}

	return Context->CompleteTaskExecution();
	*/
}

#undef LOCTEXT_NAMESPACE
