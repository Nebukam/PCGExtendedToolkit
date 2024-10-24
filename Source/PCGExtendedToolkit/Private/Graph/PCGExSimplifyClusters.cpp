// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExSimplifyClusters.h"


#include "Graph/PCGExChain.h"
#include "Graph/Filters/PCGExClusterFilter.h"


#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExSimplifyClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }
PCGExData::EInit UPCGExSimplifyClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

#pragma endregion

PCGEX_INITIALIZE_ELEMENT(SimplifyClusters)

bool FPCGExSimplifyClustersElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SimplifyClusters)

	PCGEX_FWD(GraphBuilderDetails)

	return true;
}

bool FPCGExSimplifyClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSimplifyClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SimplifyClusters)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatchWithGraphBuilder<PCGExSimplifyClusters::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::TBatchWithGraphBuilder<PCGExSimplifyClusters::FProcessor>>& NewBatch)
			{
			}))
		{
			Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExGraph::State_ReadyToCompile)
	if (!Context->CompileGraphBuilders(true, PCGEx::State_Done)) { return false; }

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExSimplifyClusters
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSimplifyClusters::Process);


		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		Breakpoints.Init(false, Cluster->Nodes->Num());

		if (!Context->FilterFactories.IsEmpty())
		{
			const TSharedPtr<PCGExClusterFilter::FManager> FilterManager = MakeShared<PCGExClusterFilter::FManager>(Cluster.ToSharedRef(), VtxDataFacade, EdgeDataFacade);
			FilterManager->Init(ExecutionContext, Context->FilterFactories);
			for (const PCGExCluster::FNode& Node : *Cluster->Nodes) { Breakpoints[Node.NodeIndex] = Node.IsComplex() ? true : FilterManager->Test(Node); }
		}
		else
		{
			for (const PCGExCluster::FNode& Node : *Cluster->Nodes) { Breakpoints[Node.NodeIndex] = Node.IsComplex(); }
		}

		if (IsTrivial())
		{
			AsyncManager->StartSynchronous<PCGExClusterTask::FFindNodeChains>(
				EdgeDataFacade->Source->IOIndex, nullptr, Cluster,
				&Breakpoints, &Chains, false, false);
		}
		else
		{
			AsyncManager->Start<PCGExClusterTask::FFindNodeChains>(
				EdgeDataFacade->Source->IOIndex, nullptr, Cluster,
				&Breakpoints, &Chains, false, false);
		}

		return true;
	}


	void FProcessor::CompleteWork()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSimplifyClusters::FProcessor::CompleteWork);
		PCGExClusterTask::DedupeChains(Chains);
		StartParallelLoopForRange(Chains.Num());
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 Count)
	{
		const TSharedPtr<PCGExCluster::FNodeChain> Chain = Chains[Iteration];
		if (!Chain) { return; }

		const TArray<PCGExCluster::FNode>& NodesRef = *Cluster->Nodes;
		const TArray<PCGExGraph::FIndexedEdge>& EdgesRef = *Cluster->Edges;

		const bool bIsDeadEnd = NodesRef[Chain->First].Adjacency.Num() == 1 || NodesRef[Chain->Last].Adjacency.Num() == 1;

		if (Settings->bPruneDeadEnds && bIsDeadEnd) { return; }

		if (Settings->bOperateOnDeadEndsOnly && !bIsDeadEnd)
		{
			// Dump edges
			if (Chain->SingleEdge != -1) { GraphBuilder->Graph->InsertEdge(EdgesRef[Chain->SingleEdge]); }
			else { for (const int32 EdgeIndex : Chain->Edges) { GraphBuilder->Graph->InsertEdge(EdgesRef[EdgeIndex]); } }
			return;
		}

		const int32 IOIndex = Cluster->EdgesIO.Pin()->IOIndex;
		const double DotThreshold = PCGExMath::DegreesToDot(Settings->AngularThreshold);
		PCGExGraph::FIndexedEdge NewEdge = PCGExGraph::FIndexedEdge{};

		if (!Settings->bMergeAboveAngularThreshold)
		{
			GraphBuilder->Graph->InsertEdge(
				NodesRef[Chain->First].PointIndex,
				NodesRef[Chain->Last].PointIndex,
				NewEdge, IOIndex);

			return;
		}

		if (Chain->SingleEdge != -1)
		{
			GraphBuilder->Graph->InsertEdge(EdgesRef[Chain->SingleEdge]);
			return;
		}


		int32 StartIndex = Chain->First;
		const int32 LastIndex = Chain->Nodes.Num() - 1;

		TSet<uint64> NewEdges;

		for (int i = 0; i < Chain->Nodes.Num(); i++)
		{
			const int32 CurrentIndex = Chain->Nodes[i];

			const PCGExCluster::FNode& CurrentNode = NodesRef[CurrentIndex];
			const PCGExCluster::FNode& PrevNode = i == 0 ? NodesRef[Chain->First] : NodesRef[Chain->Nodes[i - 1]];
			const PCGExCluster::FNode& NextNode = i == LastIndex ? NodesRef[Chain->Last] : NodesRef[Chain->Nodes[i + 1]];

			const FVector A = Cluster->GetDir(PrevNode, CurrentNode).GetSafeNormal();
			const FVector B = Cluster->GetDir(CurrentNode, NextNode).GetSafeNormal();

			if (!Settings->bInvertAngularThreshold) { if (FVector::DotProduct(A, B) > DotThreshold) { continue; } }
			else { if (FVector::DotProduct(A, B) < DotThreshold) { continue; } }

			NewEdges.Add(PCGEx::H64U(NodesRef[StartIndex].PointIndex, NodesRef[CurrentIndex].PointIndex));
			StartIndex = CurrentIndex;
		}

		NewEdges.Add(PCGEx::H64U(NodesRef[StartIndex].PointIndex, NodesRef[Chain->Last].PointIndex)); // Wrap

		GraphBuilder->Graph->InsertEdges(NewEdges, IOIndex);
	}

	void FProcessorBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TBatch<FProcessor>::RegisterBuffersDependencies(FacadePreloader);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SimplifyClusters)
		PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, Context->FilterFactories, FacadePreloader);
	}
}


#undef LOCTEXT_NAMESPACE
