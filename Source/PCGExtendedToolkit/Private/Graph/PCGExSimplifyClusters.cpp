// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExSimplifyClusters.h"


#include "Graph/PCGExChain.h"
#include "Graph/Filters/PCGExClusterFilter.h"


#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EIOInit UPCGExSimplifyClustersSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::NewOutput; }
PCGExData::EIOInit UPCGExSimplifyClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::NoOutput; }

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
		if (!Context->StartProcessingClusters<PCGExSimplifyClusters::FProcessorBatch>(
			[&](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExSimplifyClusters::FProcessorBatch>& NewBatch)
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
		const bool bComputeMeta = Settings->EdgeUnionData.WriteAny();

		const bool bIsDeadEnd = NodesRef[Chain->First].Adjacency.Num() == 1 || NodesRef[Chain->Last].Adjacency.Num() == 1;

		if (Settings->bPruneDeadEnds && bIsDeadEnd) { return; }

		PCGExGraph::FIndexedEdge OutEdge = PCGExGraph::FIndexedEdge{};

		if (Settings->bOperateOnDeadEndsOnly && !bIsDeadEnd)
		{
			// Dump edges
			if (Chain->SingleEdge != -1)
			{
				GraphBuilder->Graph->InsertEdge(EdgesRef[Chain->SingleEdge], OutEdge);
				if (bComputeMeta) { GraphBuilder->Graph->GetOrCreateEdgeMetadata(OutEdge.EdgeIndex).UnionSize = 1; }
			}
			else
			{
				for (const int32 EdgeIndex : Chain->Edges)
				{
					GraphBuilder->Graph->InsertEdge(EdgesRef[EdgeIndex], OutEdge);
					if (bComputeMeta) { GraphBuilder->Graph->GetOrCreateEdgeMetadata(OutEdge.EdgeIndex).UnionSize = 1; }
				}
			}
			return;
		}

		const int32 IOIndex = Cluster->EdgesIO.Pin()->IOIndex;

		int32 StartIndex = Chain->First;
		const int32 LastIndex = Chain->Nodes.Num() - 1;

		if (!Settings->bMergeAboveAngularThreshold)
		{
			GraphBuilder->Graph->InsertEdge(
				NodesRef[Chain->First].PointIndex,
				NodesRef[Chain->Last].PointIndex,
				OutEdge, IOIndex);

			if (bComputeMeta) { GraphBuilder->Graph->GetOrCreateEdgeMetadata(OutEdge.EdgeIndex).UnionSize = Chain->Nodes.Num() + 1; }
			return;
		}

		if (Chain->SingleEdge != -1)
		{
			GraphBuilder->Graph->InsertEdge(EdgesRef[Chain->SingleEdge], OutEdge);
			if (bComputeMeta) { GraphBuilder->Graph->GetOrCreateEdgeMetadata(OutEdge.EdgeIndex).UnionSize = Chain->Nodes.Num() + 1; }
			return;
		}

		const double DotThreshold = PCGExMath::DegreesToDot(Settings->AngularThreshold);
		TSet<uint64> NewEdges;

		// Insert nodes incrementally, checking for angle

		TMap<uint64, int32> NewEdgesMap;
		int32 FusedEdgeNum = 1;

		for (int i = 0; i < Chain->Nodes.Num(); i++)
		{
			const int32 CurrentIndex = Chain->Nodes[i];

			const PCGExCluster::FNode& CurrentNode = NodesRef[CurrentIndex];
			const PCGExCluster::FNode& PrevNode = i == 0 ? NodesRef[Chain->First] : NodesRef[Chain->Nodes[i - 1]];
			const PCGExCluster::FNode& NextNode = i == LastIndex ? NodesRef[Chain->Last] : NodesRef[Chain->Nodes[i + 1]];

			const FVector A = Cluster->GetDir(PrevNode, CurrentNode).GetSafeNormal();
			const FVector B = Cluster->GetDir(CurrentNode, NextNode).GetSafeNormal();

			if (!Settings->bInvertAngularThreshold)
			{
				if (FVector::DotProduct(A, B) > DotThreshold)
				{
					FusedEdgeNum++;
					continue;
				}
			}
			else
			{
				if (FVector::DotProduct(A, B) < DotThreshold)
				{
					FusedEdgeNum++;
					continue;
				}
			}

			uint64 Hash = PCGEx::H64U(NodesRef[StartIndex].PointIndex, NodesRef[CurrentIndex].PointIndex);
			NewEdges.Add(Hash);
			StartIndex = CurrentIndex;

			NewEdgesMap.Add(Hash, FusedEdgeNum);
			FusedEdgeNum = 1;
		}

		NewEdges.Add(PCGEx::H64U(NodesRef[StartIndex].PointIndex, NodesRef[Chain->Last].PointIndex)); // Wrap
		GraphBuilder->Graph->InsertEdges(NewEdges, IOIndex);

		if (bComputeMeta)
		{
			for (const uint64 H : NewEdges)
			{
				GraphBuilder->Graph->GetOrCreateNodeMetadataUnsafe(GraphBuilder->Graph->FindEdge(H)->EdgeIndex).UnionSize = NewEdgesMap[H];
			}
		}
	}

	const PCGExGraph::FGraphMetadataDetails* FProcessorBatch::GetGraphMetadataDetails()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SimplifyClusters)
		if (!Settings->EdgeUnionData.WriteAny()) { return nullptr; }
		GraphMetadataDetails.Grab(Context, Settings->EdgeUnionData);
		return &GraphMetadataDetails;
	}

	void FProcessorBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TBatch<FProcessor>::RegisterBuffersDependencies(FacadePreloader);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SimplifyClusters)
		PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, Context->FilterFactories, FacadePreloader);
	}
}


#undef LOCTEXT_NAMESPACE
