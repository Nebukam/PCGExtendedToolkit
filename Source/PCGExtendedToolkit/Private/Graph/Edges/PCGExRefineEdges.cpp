// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExRefineEdges.h"

#include "Graph/PCGExGraph.h"
#include "Graph/Edges/Refining/PCGExEdgeRefineUrquhart.h"

#define LOCTEXT_NAMESPACE "PCGExRefineEdges"
#define PCGEX_NAMESPACE RefineEdges

UPCGExRefineEdgesSettings::UPCGExRefineEdgesSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

PCGExData::EInit UPCGExRefineEdgesSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

bool UPCGExRefineEdgesSettings::GetCacheAllClusters() const { return true; }

PCGEX_INITIALIZE_ELEMENT(RefineEdges)

FPCGExRefineEdgesContext::~FPCGExRefineEdgesContext()
{
	PCGEX_TERMINATE_ASYNC
	PCGEX_DELETE(NetworkBuilder)
}

bool FPCGExRefineEdgesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(RefineEdges)

	PCGEX_OPERATION_BIND(Refinement, UPCGExEdgeRefineUrquhart)

	return true;
}

bool FPCGExRefineEdgesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExRefineEdgesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(RefineEdges)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		PCGEX_DELETE(Context->NetworkBuilder)

		if (!Context->AdvanceAndBindPointsIO()) { Context->Done(); }
		else
		{
			if (!Context->BoundEdges->IsValid())
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points have no bound edges."));
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
			}
			else
			{
				Context->NetworkBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, 8);
				if (Settings->bPruneIsolatedPoints) { Context->NetworkBuilder->EnablePointsPruning(); }

				Context->SetState(PCGExGraph::State_ReadyForNextEdges);
			}
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		while (Context->AdvanceEdges())
		{
			/* Batch-build all meshes since bCacheAllClusters == true */
			if (Context->CurrentCluster->HasInvalidEdges())
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input edges are invalid. This will highly likely cause unexpected results."));
			}

			Context->GetAsyncManager()->Start<FRefineEdgesTask>(-1, Context->CurrentIO, Context->CurrentCluster, Context->CurrentEdges);
		}

		Context->SetAsyncState(PCGExGraph::State_ProcessingEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		if (Context->NetworkBuilder->Compile(Context))
		{
			Context->SetAsyncState(PCGExGraph::State_WaitingOnWritingClusters);
		}
		else
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
	}

	if (Context->IsState(PCGExGraph::State_WaitingOnWritingClusters))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
		Context->NetworkBuilder->Write(Context);
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
	}

	return Context->IsDone();
}

bool FRefineEdgesTask::ExecuteTask()
{
	return false;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
