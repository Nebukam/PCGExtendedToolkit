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

bool FPCGExRefineEdgesContext::DefaultVtxFilterResult() const { return false; }

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

		Context->StartProcessingClusters<PCGExRefineEdges::FRefineClusterBatch>(
			[&](PCGExRefineEdges::FRefineClusterBatch* NewBatch)
			{
				NewBatch->Refinement = Context->Refinement;
			}, PCGExMT::State_Done);
	}

	if (!Context->ProcessClusters()) { return false; }

	if (Context->IsDone())
	{
		Context->OutputMainPoints();
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}

namespace PCGExRefineEdges
{
	bool FPCGExRefineEdgesTask::ExecuteTask()
	{
		const FPCGExRefineEdgesContext* Context = Manager->GetContext<FPCGExRefineEdgesContext>();
		Refinement->Process(Cluster, HeuristicsHandler);
		return false;
	}

	FClusterRefineProcess::FClusterRefineProcess(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges)
		: FClusterProcessingData(InVtx, InEdges)
	{
		bRequiresHeuristics = true;
	}

	FClusterRefineProcess::~FClusterRefineProcess()
	{
	}

	bool FClusterRefineProcess::Process(FPCGExAsyncManager* AsyncManager)
	{
		if (!FClusterProcessingData::Process(AsyncManager)) { return false; }

		if (IsTrivial())
		{
			Refinement->Process(Cluster, HeuristicsHandler);
			return true;
		}

		AsyncManagerPtr->Start<FPCGExRefineEdgesTask>(-1, nullptr, Cluster, Refinement, HeuristicsHandler);
		return true;
	}

	void FClusterRefineProcess::CompleteWork()
	{
		PCGEX_SETTINGS(RefineEdges)

		TArray<PCGExGraph::FIndexedEdge> ValidEdges;
		Cluster->GetValidEdges(ValidEdges);
		GraphBuilder->Graph->InsertEdges(ValidEdges);

		FClusterProcessingData::CompleteWork();
	}

	FRefineClusterBatch::FRefineClusterBatch(FPCGContext* InContext, PCGExData::FPointIO* InVtx, const TArrayView<PCGExData::FPointIO*> InEdges)
		: TClusterBatchBuilderProcessor(InContext, InVtx, InEdges)
	{
	}

	bool FRefineClusterBatch::PrepareSingle(FClusterRefineProcess* ClusterProcessor)
	{
		ClusterProcessor->Refinement = Refinement;
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
