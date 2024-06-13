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
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		while (Context->AdvancePointsIO(false))
		{
			if (!Context->TaggedEdges)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points have no bound edges."));
				return false;
			}

			PCGExRefineEdges::FRefineClusterBatch* NewBatch = new PCGExRefineEdges::FRefineClusterBatch(Context, Context->CurrentIO, Context->TaggedEdges->Entries);
			NewBatch->Refinement = Context->Refinement;
			NewBatch->MainEdges = Context->MainEdges;
			NewBatch->SetVtxFilterData(Context->VtxFiltersData, false);
			Context->Batches.Add(NewBatch);
			PCGExClusterBatch::ScheduleBatch(Context->GetAsyncManager(), NewBatch);
		}

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC

		PCGExClusterBatch::CompleteBatches<PCGExRefineEdges::FRefineClusterBatch>(Context->GetAsyncManager(), Context->Batches);
		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncCompletion);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncCompletion))
	{
		PCGEX_WAIT_ASYNC

		for (const PCGExRefineEdges::FRefineClusterBatch* Batch : Context->Batches) { Batch->GraphBuilder->Compile(Context); }
		Context->SetAsyncState(PCGExGraph::State_Compiling);
	}

	if (Context->IsState(PCGExGraph::State_Compiling))
	{
		PCGEX_WAIT_ASYNC
		for (const PCGExRefineEdges::FRefineClusterBatch* Batch : Context->Batches)
		{
			if (Batch->GraphBuilder->bCompiledSuccessfully) { Batch->GraphBuilder->Write(Context); }
		}

		Context->Done();
	}

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

	PCGExRefineEdges::FClusterRefineProcess::FClusterRefineProcess(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges)
		: FClusterProcessingData(InVtx, InEdges)
	{
		bRequiresHeuristics = true;
	}

	FClusterRefineProcess::~FClusterRefineProcess()
	{
	}

	bool PCGExRefineEdges::FClusterRefineProcess::Process(FPCGExAsyncManager* AsyncManager)
	{
		if (!FClusterProcessingData::Process(AsyncManager)) { return false; }
		AsyncManager->Start<FPCGExRefineEdgesTask>(-1, nullptr, Cluster, Refinement, HeuristicsHandler);
		return true;
	}

	void FClusterRefineProcess::CompleteWork(FPCGExAsyncManager* AsyncManager)
	{
		PCGEX_SETTINGS(RefineEdges)

		TArray<PCGExGraph::FIndexedEdge> ValidEdges;
		Cluster->GetValidEdges(ValidEdges);
		GraphBuilder->Graph->InsertEdges(ValidEdges);

		FClusterProcessingData::CompleteWork(AsyncManager);
	}

	FRefineClusterBatch::FRefineClusterBatch(FPCGContext* InContext, PCGExData::FPointIO* InVtx, TArrayView<PCGExData::FPointIO*> InEdges)
		: FClusterBatchProcessingData(InContext, InVtx, InEdges)
	{
	}

	FRefineClusterBatch::~FRefineClusterBatch()
	{
		PCGEX_DELETE(GraphBuilder)
	}

	bool PCGExRefineEdges::FRefineClusterBatch::PrepareProcessing()
	{
		PCGEX_SETTINGS(RefineEdges)

		if (!FClusterBatchProcessingData::PrepareProcessing()) { return false; }

		GraphBuilder = new PCGExGraph::FGraphBuilder(*VtxIO, &GraphBuilderSettings, 6, MainEdges);

		return true;
	}

	bool FRefineClusterBatch::PrepareSingle(FClusterRefineProcess* ClusterProcessor)
	{
		ClusterProcessor->GraphBuilder = GraphBuilder;
		ClusterProcessor->Refinement = Refinement;
		return true;
	}

	void FRefineClusterBatch::CompleteWork(FPCGExAsyncManager* AsyncManager)
	{
		FClusterBatchProcessingData<FClusterRefineProcess>::CompleteWork(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
