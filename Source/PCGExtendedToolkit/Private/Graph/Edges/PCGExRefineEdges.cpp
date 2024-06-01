// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExRefineEdges.h"

#include "Graph/PCGExGraph.h"
#include "Graph/Edges/Refining/PCGExEdgeRefinePrimMST.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"

#define LOCTEXT_NAMESPACE "PCGExRefineEdges"
#define PCGEX_NAMESPACE RefineEdges

void UPCGExRefineEdgesSettings::PostInitProperties()
{
	Super::PostInitProperties();
	PCGEX_OPERATION_DEFAULT(Refinement, UPCGExEdgeRefinePrimMST)
}

TArray<FPCGPinProperties> UPCGExRefineEdgesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExPathfinding::SourceHeuristicsLabel, "Heuristics may be used by some refinements, such as MST.", Required, {})
	return PinProperties;
}

PCGExData::EInit UPCGExRefineEdgesSettings::GetMainOutputInitMode() const { return GraphBuilderSettings.bPruneIsolatedPoints ? PCGExData::EInit::NewOutput : PCGExData::EInit::DuplicateInput; }
PCGExData::EInit UPCGExRefineEdgesSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(RefineEdges)

FPCGExRefineEdgesContext::~FPCGExRefineEdgesContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(HeuristicsHandler)
	PCGEX_DELETE(GraphBuilder)
}

bool FPCGExRefineEdgesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(RefineEdges)

	PCGEX_OPERATION_BIND(Refinement, UPCGExEdgeRefinePrimMST)
	PCGEX_FWD(GraphBuilderSettings)

	Context->HeuristicsHandler = new PCGExHeuristics::THeuristicsHandler(Context);

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
		PCGEX_DELETE(Context->GraphBuilder)

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges) { return false; }

			Context->Refinement->PrepareForPointIO(Context->CurrentIO);
			Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, &Context->GraphBuilderSettings, 8, Context->MainEdges);
			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		if (!Context->AdvanceEdges(true))
		{
			Context->GraphBuilder->Compile(Context);
			Context->SetAsyncState(PCGExGraph::State_WaitingOnWritingClusters);
		}
		else
		{
			if (!Context->CurrentCluster)
			{
				PCGEX_INVALID_CLUSTER_LOG
				return false;
			}

			Context->Refinement->PreProcess(Context->CurrentCluster, Context->GraphBuilder->Graph, Context->CurrentEdges);

			if (!Context->HeuristicsHandler->PrepareForCluster(Context->GetAsyncManager(), Context->CurrentCluster))
			{
				Context->GetAsyncManager()->Start<FPCGExRefineEdgesTask>(-1, Context->CurrentIO, Context->CurrentCluster, Context->CurrentEdges);
				Context->SetAsyncState(PCGExGraph::State_ProcessingEdges);
			}
			else
			{
				Context->SetAsyncState(PCGExPathfinding::State_ProcessingHeuristics);
			}
		}
	}

	if (Context->IsState(PCGExPathfinding::State_ProcessingHeuristics))
	{
		PCGEX_WAIT_ASYNC

		Context->HeuristicsHandler->CompleteClusterPreparation();

		Context->GetAsyncManager()->Start<FPCGExRefineEdgesTask>(-1, Context->CurrentIO, Context->CurrentCluster, Context->CurrentEdges);
		Context->SetAsyncState(PCGExGraph::State_ProcessingEdges);

		return false;
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		PCGEX_WAIT_ASYNC

		Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		return false;
	}

	if (Context->IsState(PCGExGraph::State_WaitingOnWritingClusters))
	{
		PCGEX_WAIT_ASYNC

		if (Context->GraphBuilder->bCompiledSuccessfully) { Context->GraphBuilder->Write(Context); }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
	}

	return Context->IsDone();
}

bool FPCGExRefineEdgesTask::ExecuteTask()
{
	const FPCGExRefineEdgesContext* Context = Manager->GetContext<FPCGExRefineEdgesContext>();
	Context->Refinement->Process(Cluster, Context->GraphBuilder->Graph, EdgeIO, Context->HeuristicsHandler);
	return false;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
