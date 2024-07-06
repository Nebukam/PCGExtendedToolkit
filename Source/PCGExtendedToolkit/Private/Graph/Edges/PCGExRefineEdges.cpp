// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExRefineEdges.h"

#include "Graph/PCGExGraph.h"
#include "Graph/Edges/Refining/PCGExEdgeRefinePrimMST.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"

#define LOCTEXT_NAMESPACE "PCGExRefineEdges"
#define PCGEX_NAMESPACE RefineEdges

TArray<FPCGPinProperties> UPCGExRefineEdgesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (Refinement && Refinement->RequiresHeuristics()) { PCGEX_PIN_PARAMS(PCGExGraph::SourceHeuristicsLabel, "Heuristics may be required by some refinements.", Required, {}) }
	PCGEX_PIN_PARAMS(PCGExRefineEdges::SourceProtectEdgeFilters, "Filters used to preserve specific edges.", Advanced, {})
	return PinProperties;
}

PCGExData::EInit UPCGExRefineEdgesSettings::GetMainOutputInitMode() const { return GraphBuilderDetails.bPruneIsolatedPoints ? PCGExData::EInit::NewOutput : PCGExData::EInit::DuplicateInput; }
PCGExData::EInit UPCGExRefineEdgesSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(RefineEdges)

FPCGExRefineEdgesContext::~FPCGExRefineEdgesContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExRefineEdgesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(RefineEdges)

	if (!Settings->Refinement)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("No refinement selected."));
		return false;
	}

	PCGEX_OPERATION_BIND(Refinement, UPCGExEdgeRefinePrimMST)
	PCGEX_FWD(GraphBuilderDetails)

	if (Context->Refinement->RequiresHeuristics() && !Context->bHasValidHeuristics)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("The selected refinement requires heuristics to be connected, but none can be found."));
		return false;
	}

	PCGExFactories::GetInputFactories(Context, PCGExRefineEdges::SourceProtectEdgeFilters, Context->PreserveEdgeFilterFactories, PCGExFactories::PointFilters, false);

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

		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatchWithGraphBuilder<PCGExRefineEdges::FProcessor>>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExClusterMT::TBatchWithGraphBuilder<PCGExRefineEdges::FProcessor>* NewBatch)
			{
				NewBatch->GraphBuilderDetails = Context->GraphBuilderDetails;
				if (Context->Refinement->RequiresHeuristics()) { NewBatch->SetRequiresHeuristics(true); }
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	if (Context->IsDone())
	{
		Context->OutputMainPoints();
	}

	return Context->TryComplete();
}

namespace PCGExRefineEdges
{
	PCGExCluster::FCluster* FProcessor::HandleCachedCluster(const PCGExCluster::FCluster* InClusterRef)
	{
		// Create a light working copy with edges only, will be deleted.
		bDeleteCluster = true;
		return new PCGExCluster::FCluster(InClusterRef, VtxIO, EdgesIO, false, true, false);
	}

	FProcessor::~FProcessor()
	{
		PCGEX_DELETE_OPERATION(Refinement)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(RefineEdges)

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		Refinement = TypedContext->Refinement->CopyOperation<UPCGExEdgeRefineOperation>();
		Refinement->PrepareForCluster(Cluster, HeuristicsHandler);

		if (Refinement->RequiresIndividualNodeProcessing()) { StartParallelLoopForNodes(); }
		else if (Refinement->RequiresIndividualEdgeProcessing()) { StartParallelLoopForEdges(); }
		else { Refinement->Process(); }
		return true;
	}

	void FProcessor::ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node) { Refinement->ProcessNode(Node); }

	void FProcessor::ProcessSingleEdge(PCGExGraph::FIndexedEdge& Edge) { Refinement->ProcessEdge(Edge); }

	void FProcessor::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(RefineEdges)

		if (!TypedContext->PreserveEdgeFilterFactories.IsEmpty())
		{
			PCGExPointFilter::TManager* FilterManager = new PCGExPointFilter::TManager(EdgeDataFacade);
			FilterManager->Init(Context, TypedContext->PreserveEdgeFilterFactories);
			for (PCGExGraph::FIndexedEdge& Edge : *Cluster->Edges) { if (!Edge.bValid) { Edge.bValid = FilterManager->Test(Edge.EdgeIndex); } }
			PCGEX_DELETE(FilterManager);
		}

		TArray<PCGExGraph::FIndexedEdge> ValidEdges;
		Cluster->GetValidEdges(ValidEdges);
		GraphBuilder->Graph->InsertEdges(ValidEdges);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
