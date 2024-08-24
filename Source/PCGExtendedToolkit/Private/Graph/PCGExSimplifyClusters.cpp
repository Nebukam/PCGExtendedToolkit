// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExSimplifyClusters.h"

#include "Graph/Filters/PCGExClusterFilter.h"


#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExSimplifyClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }
PCGExData::EInit UPCGExSimplifyClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

TArray<FPCGPinProperties> UPCGExSimplifyClustersSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExPointFilter::SourceFiltersLabel, "Anchor points (won't be affected by the simplification)", Advanced, {})
	return PinProperties;
}

#pragma endregion

FPCGExSimplifyClustersContext::~FPCGExSimplifyClustersContext()
{
	PCGEX_TERMINATE_ASYNC
}

PCGEX_INITIALIZE_ELEMENT(SimplifyClusters)

bool FPCGExSimplifyClustersElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SimplifyClusters)

	PCGEX_FWD(GraphBuilderDetails)
	Context->GraphBuilderDetails.bPruneIsolatedPoints = true;

	GetInputFactories(
		InContext, PCGExPointFilter::SourceFiltersLabel, Context->FilterFactories,
		PCGExFactories::ClusterNodeFilters, false);

	return true;
}

bool FPCGExSimplifyClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSimplifyClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SimplifyClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatchWithGraphBuilder<PCGExSimplifyClusters::FProcessor>>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExClusterMT::TBatchWithGraphBuilder<PCGExSimplifyClusters::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExSimplifyClusters
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE_TARRAY(Chains)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSimplifyClusters::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SimplifyClusters)

		LocalTypedContext = TypedContext;
		LocalSettings = Settings;

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		PCGEX_SET_NUM_UNINITIALIZED(Breakpoints, Cluster->Nodes->Num())

		if (!TypedContext->FilterFactories.IsEmpty())
		{
			PCGExClusterFilter::TManager* FilterManager = new PCGExClusterFilter::TManager(Cluster, VtxDataFacade, EdgeDataFacade);
			FilterManager->Init(Context, TypedContext->FilterFactories);
			for (const PCGExCluster::FNode& Node : *Cluster->Nodes) { Breakpoints[Node.NodeIndex] = Node.IsComplex() ? true : FilterManager->Test(Node); }
			PCGEX_DELETE(FilterManager)
		}
		else
		{
			for (const PCGExCluster::FNode& Node : *Cluster->Nodes) { Breakpoints[Node.NodeIndex] = Node.IsComplex(); }
		}

		if (IsTrivial())
		{
			AsyncManagerPtr->StartSynchronous<PCGExClusterTask::FFindNodeChains>(
				EdgesIO->IOIndex, nullptr, Cluster,
				&Breakpoints, &Chains, false, false);
		}
		else
		{
			AsyncManagerPtr->Start<PCGExClusterTask::FFindNodeChains>(
				EdgesIO->IOIndex, nullptr, Cluster,
				&Breakpoints, &Chains, false, false);
		}

		return true;
	}


	void FProcessor::CompleteWork()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSimplifyClusters::FProcessor::CompleteWork);

		PCGEX_SETTINGS(SimplifyClusters)

		PCGExClusterTask::DedupeChains(Chains);
		StartParallelLoopForRange(Chains.Num());
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration)
	{
		PCGExCluster::FNodeChain* Chain = Chains[Iteration];
		if (!Chain) { return; }

		const TArray<PCGExCluster::FNode>& NodesRef = *Cluster->Nodes;
		const TArray<PCGExGraph::FIndexedEdge>& EdgesRef = *Cluster->Edges;

		const bool bIsDeadEnd = NodesRef[Chain->First].Adjacency.Num() == 1 || NodesRef[Chain->Last].Adjacency.Num() == 1;

		if (LocalSettings->bPruneDeadEnds && bIsDeadEnd) { return; }

		if (LocalSettings->bOperateOnDeadEndsOnly && !bIsDeadEnd)
		{
			// Dump edges
			if (Chain->SingleEdge != -1) { GraphBuilder->Graph->InsertEdge(EdgesRef[Chain->SingleEdge]); }
			else { for (const int32 EdgeIndex : Chain->Edges) { GraphBuilder->Graph->InsertEdge(EdgesRef[EdgeIndex]); } }
			return;
		}

		const int32 IOIndex = Cluster->EdgesIO->IOIndex;
		const double FixedDotThreshold = PCGExMath::DegreesToDot(LocalSettings->AngularThreshold * 0.5);
		PCGExGraph::FIndexedEdge NewEdge = PCGExGraph::FIndexedEdge{};

		if (!LocalSettings->bMergeAboveAngularThreshold)
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

			if (!LocalSettings->bInvertAngularThreshold) { if (FVector::DotProduct(A, B) > FixedDotThreshold) { continue; } }
			else { if (FVector::DotProduct(A, B) < FixedDotThreshold) { continue; } }

			NewEdges.Add(PCGEx::H64U(NodesRef[StartIndex].PointIndex, NodesRef[CurrentIndex].PointIndex));
			StartIndex = CurrentIndex;
		}

		NewEdges.Add(PCGEx::H64U(NodesRef[StartIndex].PointIndex, NodesRef[Chain->Last].PointIndex)); // Wrap

		GraphBuilder->Graph->InsertEdges(NewEdges, IOIndex);
	}
}


#undef LOCTEXT_NAMESPACE
