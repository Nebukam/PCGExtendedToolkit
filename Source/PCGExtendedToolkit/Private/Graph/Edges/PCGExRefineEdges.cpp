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
	if (Refinement && Refinement->RequiresHeuristics()) { PCGEX_PIN_PARAMS(PCGExGraph::SourceHeuristicsLabel, "Heuristics may be required by some refinements.", Required, {}) }
	return PinProperties;
}

PCGExData::EInit UPCGExRefineEdgesSettings::GetMainOutputInitMode() const { return GraphBuilderSettings.bPruneIsolatedPoints ? PCGExData::EInit::NewOutput : PCGExData::EInit::DuplicateInput; }
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
	PCGEX_FWD(GraphBuilderSettings)

	if (Context->Refinement->RequiresHeuristics() && !Context->bHasValidHeuristics)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("The selected refinement requires heuristics to be connected, but none can be found."));
		return false;
	}

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

		if (!Context->StartProcessingClusters<PCGExRefineEdges::FProcessorBatch>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExRefineEdges::FProcessorBatch* NewBatch)
			{
				NewBatch->Refinement = Context->Refinement;
				if (NewBatch->Refinement->RequiresHeuristics()) { NewBatch->SetRequiresHeuristics(true); }
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
	bool FRefineTask::ExecuteTask()
	{
		const FPCGExRefineEdgesContext* Context = Manager->GetContext<FPCGExRefineEdgesContext>();
		Refinement->Process(Cluster, HeuristicsHandler);
		return false;
	}

	PCGExCluster::FCluster* FProcessor::HandleCachedCluster(const PCGExCluster::FCluster* InClusterRef)
	{
		// Create a light working copy with edges only, will be deleted.
		bDeleteCluster = true;
		return new PCGExCluster::FCluster(InClusterRef, VtxIO, EdgesIO, false, true, false);
	}

	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		if (IsTrivial())
		{
			Refinement->Process(Cluster, HeuristicsHandler);
			return true;
		}

		AsyncManagerPtr->Start<FRefineTask>(-1, nullptr, Cluster, Refinement, HeuristicsHandler);
		return true;
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_SETTINGS(RefineEdges)

		TArray<PCGExGraph::FIndexedEdge> ValidEdges;
		Cluster->GetValidEdges(ValidEdges);
		GraphBuilder->Graph->InsertEdges(ValidEdges);
	}

	FProcessorBatch::FProcessorBatch(FPCGContext* InContext, PCGExData::FPointIO* InVtx, const TArrayView<PCGExData::FPointIO*> InEdges)
		: TBatchWithGraphBuilder<FProcessor>(InContext, InVtx, InEdges)
	{
	}

	bool FProcessorBatch::PrepareSingle(FProcessor* ClusterProcessor)
	{
		ClusterProcessor->Refinement = Refinement;
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
