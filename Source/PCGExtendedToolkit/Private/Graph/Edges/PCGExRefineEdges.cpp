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
	PCGEX_PIN_PARAMS(PCGExPathfinding::SourceHeuristicsLabel, "Heuristics may be used by some refinements, such as MST.", Normal, {})
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

	PCGEX_OPERATION_BIND(Refinement, UPCGExEdgeRefinePrimMST)
	PCGEX_FWD(GraphBuilderSettings)

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
		Context->ExecuteEnd();
	}

	return Context->IsDone();
}

namespace PCGExRefineEdges
{
	bool FRefineTask::ExecuteTask()
	{
		const FPCGExRefineEdgesContext* Context = Manager->GetContext<FPCGExRefineEdgesContext>();
		Refinement->Process(Cluster, HeuristicsHandler);
		return false;
	}

	FProcessor::FProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges)
		: FClusterProcessor(InVtx, InEdges)
	{
		bRequiresHeuristics = true;
		DefaultVtxFilterValue = false;
	}

	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(FPCGExAsyncManager* AsyncManager)
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
		: TBatchWithGraphBuilder(InContext, InVtx, InEdges)
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
