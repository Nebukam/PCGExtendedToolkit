// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExSimplifyClusters.h"

#include "Data/PCGExGraphDefinition.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExSimplifyClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }
PCGExData::EInit UPCGExSimplifyClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FName UPCGExSimplifyClustersSettings::GetVtxFilterLabel() const { return PCGExDataFilter::SourceFiltersLabel; }

#pragma endregion

FPCGExSimplifyClustersContext::~FPCGExSimplifyClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(GraphBuilder)

	PCGEX_DELETE_TARRAY(Chains)
}

PCGEX_INITIALIZE_ELEMENT(SimplifyClusters)

bool FPCGExSimplifyClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SimplifyClusters)

	Context->GraphBuilderSettings.bPruneIsolatedPoints = true;
	Context->FixedDotThreshold = PCGExMath::DegreesToDot(Settings->AngularThreshold);

	return true;
}

bool FPCGExSimplifyClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSimplifyClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SimplifyClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (!Context->ProcessorAutomation()) { return false; }


	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		while (Context->AdvancePointsIO(false))
		{
			if (!Context->TaggedEdges)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points have no bound edges."));
				return false;
			}

			PCGExSimplifyClusters::FSimplifyClusterBatch* NewBatch = new PCGExSimplifyClusters::FSimplifyClusterBatch(Context, Context->CurrentIO, Context->TaggedEdges->Entries);
			NewBatch->SetVtxFilterData(Context->VtxFiltersData, false);
			NewBatch->MainEdges = Context->MainEdges;
			Context->Batches.Add(NewBatch);
			PCGExClusterBatch::ScheduleBatch(Context->GetAsyncManager(), NewBatch);
		}

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC

		PCGExClusterBatch::CompleteBatches<PCGExSimplifyClusters::FSimplifyClusterBatch>(Context->GetAsyncManager(), Context->Batches);
		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncCompletion);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncCompletion))
	{
		PCGEX_WAIT_ASYNC

		for (const PCGExSimplifyClusters::FSimplifyClusterBatch* Batch : Context->Batches) { Batch->GraphBuilder->Compile(Context); }
		Context->SetAsyncState(PCGExGraph::State_Compiling);
	}

	if (Context->IsState(PCGExGraph::State_Compiling))
	{
		PCGEX_WAIT_ASYNC
		for (const PCGExSimplifyClusters::FSimplifyClusterBatch* Batch : Context->Batches)
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

namespace PCGExSimplifyClusters
{
	FClusterSimplifyProcess::FClusterSimplifyProcess(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges):
		FClusterProcessingData(InVtx, InEdges)
	{
	}

	FClusterSimplifyProcess::~FClusterSimplifyProcess()
	{
		PCGEX_DELETE_TARRAY(Chains)
	}

	bool FClusterSimplifyProcess::Process(FPCGExAsyncManager* AsyncManager)
	{
		if (!FClusterProcessingData::Process(AsyncManager)) { return false; }

		PCGEX_SETTINGS(SimplifyClusters)

		for (int i = 0; i < Cluster->Nodes.Num(); i++) { if (Cluster->Nodes[i].IsComplex()) { VtxFilterCache[i] = true; } }

		AsyncManager->Start<PCGExClusterTask::FFindNodeChains>(
			EdgesIO->IOIndex, nullptr, Cluster,
			&VtxFilterCache, &Chains, false, Settings->bOperateOnDeadEndsOnly);

		return true;
	}


	void FClusterSimplifyProcess::CompleteWork(FPCGExAsyncManager* AsyncManager)
	{
		PCGEX_SETTINGS(SimplifyClusters)

		PCGExClusterTask::DedupeChains(Chains);
		StartParallelLoopForRange(AsyncManager, Chains.Num());

		FClusterProcessingData::CompleteWork(AsyncManager);
	}

	void FClusterSimplifyProcess::ProcessSingleRangeIteration(const int32 Iteration)
	{
		PCGEX_SETTINGS(SimplifyClusters)

		PCGExCluster::FNodeChain* Chain = Chains[Iteration];
		if (!Chain) { return; }

		TArray<PCGExCluster::FNode>& Nodes = Cluster->Nodes;
		TArray<PCGExGraph::FIndexedEdge>& Edges = Cluster->Edges;
		const int32 IOIndex = Cluster->EdgesIO->IOIndex;

		const double FixedDotThreshold = PCGExMath::DegreesToDot(Settings->AngularThreshold);

		const bool bIsDeadEnd = Nodes[Chain->First].Adjacency.Num() == 1 || Nodes[Chain->Last].Adjacency.Num() == 1;

		if (Settings->bPruneDeadEnds && bIsDeadEnd) { return; }

		PCGExGraph::FIndexedEdge NewEdge = PCGExGraph::FIndexedEdge{};

		if (!Settings->bMergeAboveAngularThreshold)
		{
			GraphBuilder->Graph->InsertEdge(
				Nodes[Chain->First].PointIndex,
				Nodes[Chain->Last].PointIndex,
				NewEdge, IOIndex);

			return;
		}

		if(Chain->SingleEdge != -1)
		{
			GraphBuilder->Graph->InsertEdge(Edges[Chain->SingleEdge]);
			return;
		}


		int32 StartIndex = Chain->First;
		const int32 LastIndex = Chain->Nodes.Num() - 1;

		TSet<uint64> NewEdges;

		for (int i = 0; i < Chain->Nodes.Num(); i++)
		{
			const int32 CurrentIndex = Chain->Nodes[i];

			PCGExCluster::FNode& CurrentNode = Nodes[CurrentIndex];
			PCGExCluster::FNode& PrevNode = i == 0 ? Nodes[Chain->First] : Nodes[Chain->Nodes[i - 1]];
			PCGExCluster::FNode& NextNode = i == LastIndex ? Nodes[Chain->Last] : Nodes[Chain->Nodes[i + 1]];

			const FVector A = (PrevNode.Position - CurrentNode.Position).GetSafeNormal();
			const FVector B = (CurrentNode.Position - NextNode.Position).GetSafeNormal();

			if (!Settings->bInvertAngularThreshold) { if (FVector::DotProduct(A, B) > FixedDotThreshold) { continue; } }
			else { if (FVector::DotProduct(A, B) < FixedDotThreshold) { continue; } }

			NewEdges.Add(PCGEx::H64U(Nodes[StartIndex].PointIndex, Nodes[CurrentIndex].PointIndex));
			StartIndex = CurrentIndex;
		}

		NewEdges.Add(PCGEx::H64U(Nodes[StartIndex].PointIndex, Nodes[Chain->Last].PointIndex)); // Wrap

		GraphBuilder->Graph->InsertEdges(NewEdges, IOIndex);
	}

	//////// BATCH

	FSimplifyClusterBatch::FSimplifyClusterBatch(FPCGContext* InContext, PCGExData::FPointIO* InVtx, TArrayView<PCGExData::FPointIO*> InEdges):
		FClusterBatchProcessingData(InContext, InVtx, InEdges)
	{
	}

	FSimplifyClusterBatch::~FSimplifyClusterBatch()
	{
		PCGEX_DELETE(GraphBuilder)
	}

	bool FSimplifyClusterBatch::PrepareProcessing()
	{
		PCGEX_SETTINGS(SimplifyClusters)

		if (!FClusterBatchProcessingData::PrepareProcessing()) { return false; }

		GraphBuilder = new PCGExGraph::FGraphBuilder(*VtxIO, &GraphBuilderSettings, 6, MainEdges);

		return true;
	}

	bool FSimplifyClusterBatch::PrepareSingle(FClusterSimplifyProcess* ClusterProcessor)
	{
		ClusterProcessor->GraphBuilder = GraphBuilder;
		return true;
	}

	void FSimplifyClusterBatch::CompleteWork(FPCGExAsyncManager* AsyncManager)
	{
		FClusterBatchProcessingData<FClusterSimplifyProcess>::CompleteWork(AsyncManager);
	}
}


#undef LOCTEXT_NAMESPACE
