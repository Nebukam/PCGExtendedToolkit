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
			Context->Batches.Add(NewBatch);
			PCGExClusterBatch::ScheduleBatch(Context->GetAsyncManager(), NewBatch);
		}

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC

		for (PCGExSimplifyClusters::FSimplifyClusterBatch* Batch : Context->Batches) { Batch->CompleteWork(Context->GetAsyncManager()); }
		Context->Done();
	}

	///////

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		PCGEX_DELETE(Context->GraphBuilder)

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges) { return false; }
			Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, &Context->GraphBuilderSettings, 6, Context->MainEdges);
			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		if (!Context->AdvanceEdges(true))
		{
			Context->SetState(PCGExGraph::State_WritingClusters);
			return false;
		}

		if (!Context->CurrentCluster) { return false; }

		Context->SetState(PCGExDataFilter::State_FilteringPoints);
		return false; // Need to exit so we give room for point filtering
	}

	if (Context->IsState(PCGExDataFilter::State_FilteringPoints))
	{
		for (int i = 0; i < Context->CurrentCluster->Nodes.Num(); i++) { if (Context->CurrentCluster->Nodes[i].IsComplex()) { Context->VtxFilterResults[i] = true; } }

		Context->GetAsyncManager()->Start<PCGExClusterTask::FFindNodeChains>(
			Context->CurrentEdges->IOIndex, nullptr, Context->CurrentCluster,
			&Context->VtxFilterResults, &Context->Chains, true, Settings->bOperateOnDeadEndsOnly);

		Context->SetAsyncState(PCGExCluster::State_BuildingChains);
	}

	if (Context->IsState(PCGExCluster::State_BuildingChains))
	{
		PCGEX_WAIT_ASYNC

		auto Initialize = [&]() { PCGExClusterTask::DedupeChains(Context->Chains); };

		auto ProcessChain = [&](const int32 ChainIndex)
		{
			PCGExCluster::FNodeChain* Chain = Context->Chains[ChainIndex];

			if (!Chain) { return; }

			const bool bIsDeadEnd = Context->CurrentCluster->Nodes[Chain->First].Adjacency.Num() == 1 || Context->CurrentCluster->Nodes[Chain->Last].Adjacency.Num() == 1;

			if (Settings->bPruneDeadEnds && bIsDeadEnd)
			{
				// Invalidate all edges
				for (const int32 EdgeIndex : Chain->Edges) { Context->CurrentCluster->Edges[EdgeIndex].bValid = false; }
				return;
			}

			PCGExGraph::FIndexedEdge NewEdge = PCGExGraph::FIndexedEdge{};

			if (!Settings->bMergeAboveAngularThreshold)
			{
				for (const int32 EdgeIndex : Chain->Edges) { Context->CurrentCluster->Edges[EdgeIndex].bValid = false; }

				Context->GraphBuilder->Graph->InsertEdge(
					Context->CurrentCluster->Nodes[Chain->First].PointIndex,
					Context->CurrentCluster->Nodes[Chain->Last].PointIndex,
					NewEdge, Context->CurrentEdges->IOIndex);

				return;
			}


			int32 StartIndex = Chain->First;
			const int32 LastIndex = Chain->Nodes.Num() - 1;

			// Invalidate then rebuild edges
			for (const int32 EdgeIndex : Chain->Edges) { Context->CurrentCluster->Edges[EdgeIndex].bValid = false; }

			for (int i = 0; i < Chain->Nodes.Num(); i++)
			{
				const int32 CurrentIndex = Chain->Nodes[i];

				PCGExCluster::FNode& CurrentNode = Context->CurrentCluster->Nodes[CurrentIndex];
				PCGExCluster::FNode& PrevNode = i == 0 ? Context->CurrentCluster->Nodes[Chain->First] : Context->CurrentCluster->Nodes[Chain->Nodes[i - 1]];
				PCGExCluster::FNode& NextNode = i == LastIndex ? Context->CurrentCluster->Nodes[Chain->Last] : Context->CurrentCluster->Nodes[Chain->Nodes[i + 1]];

				const FVector A = (PrevNode.Position - CurrentNode.Position).GetSafeNormal();
				const FVector B = (CurrentNode.Position - NextNode.Position).GetSafeNormal();

				if (!Settings->bInvertAngularThreshold)
				{
					if (FVector::DotProduct(A, B) < Context->FixedDotThreshold)
					{
						Context->GraphBuilder->Graph->InsertEdge(
							Context->CurrentCluster->Nodes[StartIndex].PointIndex,
							Context->CurrentCluster->Nodes[CurrentIndex].PointIndex,
							NewEdge, Context->CurrentEdges->IOIndex);

						StartIndex = CurrentIndex;
					}
				}
				else
				{
					if (FVector::DotProduct(A, B) > Context->FixedDotThreshold)
					{
						Context->GraphBuilder->Graph->InsertEdge(
							Context->CurrentCluster->Nodes[StartIndex].PointIndex,
							Context->CurrentCluster->Nodes[CurrentIndex].PointIndex,
							NewEdge, Context->CurrentEdges->IOIndex);

						StartIndex = CurrentIndex;
					}
				}
			}

			Context->GraphBuilder->Graph->InsertEdge(
				Context->CurrentCluster->Nodes[StartIndex].PointIndex,
				Context->CurrentCluster->Nodes[Chain->Last].PointIndex,
				NewEdge, Context->CurrentEdges->IOIndex);
		};

		if (!Context->Process(Initialize, ProcessChain, Context->Chains.Num())) { return false; }

		PCGEX_DELETE_TARRAY(Context->Chains)

		for (const PCGExGraph::FIndexedEdge& Edge : Context->CurrentCluster->Edges)
		{
			if (!Edge.bValid) { continue; }
			Context->GraphBuilder->Graph->InsertEdge(Edge);
		}

		Context->SetState(PCGExGraph::State_ReadyForNextEdges);
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		Context->GraphBuilder->Compile(Context);
		Context->SetAsyncState(PCGExGraph::State_WaitingOnWritingClusters);
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
	}

	bool FClusterSimplifyProcess::Process(FPCGExAsyncManager* AsyncManager)
	{
		if (!FClusterProcessingData::Process(AsyncManager)) { return false; }

		PCGEX_SETTINGS(SimplifyClusters)

		for (int i = 0; i < Cluster->Nodes.Num(); i++) { if (Cluster->Nodes[i].IsComplex()) { VtxFilterCache[i] = true; } }

		AsyncManager->Start<PCGExClusterTask::FFindNodeChains>(
			Edges->IOIndex, nullptr, Cluster,
			&VtxFilterCache, &Chains, true, Settings->bOperateOnDeadEndsOnly);

		return true;
	}

	void FClusterSimplifyProcess::CompleteWork(FPCGExAsyncManager* AsyncManager)
	{
		// TODO : Insert edges to graph
		// Then compile all graphs in the context in parallel.
		FClusterProcessingData::CompleteWork(AsyncManager);
	}

	//////// BATCH

	FSimplifyClusterBatch::FSimplifyClusterBatch(FPCGContext* InContext, PCGExData::FPointIO* InVtx, TArrayView<PCGExData::FPointIO*> InEdges):
		FClusterBatchProcessingData(InContext, InVtx, InEdges)
	{
	}

	FSimplifyClusterBatch::~FSimplifyClusterBatch()
	{
	}

	bool FSimplifyClusterBatch::PrepareProcessing()
	{
		PCGEX_SETTINGS(SimplifyClusters)

		if (!FClusterBatchProcessingData::PrepareProcessing()) { return false; }

		GraphBuilder = new PCGExGraph::FGraphBuilder(*Vtx, &GraphBuilderSettings, 6, MainEdges);

		return true;
	}

	bool FSimplifyClusterBatch::PrepareSingle(FClusterSimplifyProcess* ClusterProcessor)
	{
	}

	void FSimplifyClusterBatch::CompleteWork(FPCGExAsyncManager* AsyncManager)
	{
		FClusterBatchProcessingData<FClusterSimplifyProcess>::CompleteWork(AsyncManager);
	}
}


#undef LOCTEXT_NAMESPACE
