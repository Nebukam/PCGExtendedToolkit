// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExSimplifyClusters.h"

#include "Data/PCGExGraphDefinition.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExSimplifyClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }
PCGExData::EInit UPCGExSimplifyClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

#pragma endregion

FPCGExSimplifyClustersContext::~FPCGExSimplifyClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(GraphBuilder)

	PCGEX_DELETE(IsPointFixtureGetter)
	PCGEX_DELETE(IsEdgeFixtureGetter)

	PCGEX_DELETE_TARRAY(Chains)
}

PCGEX_INITIALIZE_ELEMENT(SimplifyClusters)

bool FPCGExSimplifyClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SimplifyClusters)

	Context->GraphBuilderSettings.bPruneIsolatedPoints = true;

	if (Settings->bUseLocalNodeMark)
	{
		Context->IsPointFixtureGetter = new PCGEx::FLocalBoolGetter();
		Context->IsPointFixtureGetter->Capture(Settings->NodeFixAttribute);
	}

	if (Settings->bUseLocalEdgeMark)
	{
		Context->IsEdgeFixtureGetter = new PCGEx::FLocalBoolGetter();
		Context->IsEdgeFixtureGetter->Capture(Settings->EdgeFixAttribute);
	}

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

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		PCGEX_DELETE(Context->GraphBuilder)

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges) { return false; }

			if (Context->IsPointFixtureGetter) { Context->IsPointFixtureGetter->Grab(*Context->CurrentIO); }
			Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, &Context->GraphBuilderSettings, 6, Context->MainEdges);
			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		PCGEX_DELETE_TARRAY(Context->Chains)

		if (!Context->AdvanceEdges(true))
		{
			Context->SetState(PCGExGraph::State_WritingClusters);
			return false;
		}

		if (!Context->CurrentCluster) { return false; }

		// Insert current cluster into loose graph
		Context->GetAsyncManager()->Start<FPCGExFindClusterChainsTask>(Context->CurrentIO->IOIndex, Context->CurrentIO);
		Context->SetAsyncState(PCGExGraph::State_ProcessingGraph);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingGraph))
	{
		PCGEX_WAIT_ASYNC

		for (const PCGExGraph::FIndexedEdge& Edge : Context->CurrentCluster->Edges)
		{
			if (!Edge.bValid) { continue; }
			Context->GraphBuilder->Graph->InsertEdge(Edge);
		}

		const int32 IOIndex = Context->CurrentCluster->EdgesIO->IOIndex;

		PCGExGraph::FIndexedEdge NewEdge = PCGExGraph::FIndexedEdge{};
		for (const PCGExCluster::FNodeChain* Chain : Context->Chains)
		{
			Context->GraphBuilder->Graph->InsertEdge(
				Context->CurrentCluster->Nodes[Chain->First].PointIndex,
				Context->CurrentCluster->Nodes[Chain->Last].PointIndex, NewEdge, IOIndex);
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

bool FPCGExFindClusterChainsTask::ExecuteTask()
{
	FPCGExSimplifyClustersContext* Context = static_cast<FPCGExSimplifyClustersContext*>(Manager->Context);
	PCGEX_SETTINGS(SimplifyClusters)

	const TArray<PCGExCluster::FNode>& ClusterNodes = Context->CurrentCluster->Nodes;
	TArray<PCGExGraph::FNode>& GraphNodes = Context->GraphBuilder->Graph->Nodes;

	if (ClusterNodes.IsEmpty()) { return false; }

	TSet<int32> NodeFixtures;
	TSet<int32> VisitedNodes;
	TArray<int32> Candidates;

	bool bPruneDeadEnds = Settings->bOperateOnDeadEndsOnly || Settings->bPruneDeadEnds;
	TSet<int32> DeadEnds;

	if (bPruneDeadEnds) { DeadEnds.Reserve(ClusterNodes.Num() / 2); }

	// Find fixtures
	const PCGExCluster::FCluster& Cluster = *Context->CurrentCluster;

	if (Context->IsEdgeFixtureGetter &&
		Context->IsEdgeFixtureGetter->IsUsable(Cluster.Edges.Num()))
	{
		Context->IsEdgeFixtureGetter->Grab(*Context->CurrentEdges);
		const TArray<bool> FixedEdges = Context->IsEdgeFixtureGetter->Values;
		for (int i = 0; i < FixedEdges.Num(); i++)
		{
			if (bool bFix = FixedEdges[i]; !bFix || (bFix && Settings->bInvertEdgeFixAttribute)) { continue; }

			const PCGExGraph::FIndexedEdge& E = Cluster.Edges[i];

			NodeFixtures.Add(*Cluster.NodeIndexLookup.Find(E.Start));
			NodeFixtures.Add(*Cluster.NodeIndexLookup.Find(E.End));
		}
	}


	for (const PCGExCluster::FNode& Node : ClusterNodes)
	{
		if (Context->IsPointFixtureGetter)
		{
			if (bool bFix = Context->IsPointFixtureGetter->SafeGet(Node.PointIndex, false); !bFix || (bFix && Settings->bInvertEdgeFixAttribute))
			{
				NodeFixtures.Add(Node.NodeIndex);
				continue;
			}
		}

		if (const int32 NumNeighbors = Node.Adjacency.Num();
			NumNeighbors <= 1 || NumNeighbors > 2)
		{
			if (NumNeighbors == 1 && bPruneDeadEnds)
			{
				DeadEnds.Add(Node.NodeIndex);
				GraphNodes[Node.PointIndex].bValid = false;
				continue;
			}

			if (NumNeighbors == 0) { GraphNodes[Node.PointIndex].bValid = false; }
			NodeFixtures.Add(Node.NodeIndex);

			continue;
		}

		if (Settings->bFixBelowThreshold)
		{
			const FVector A = (Cluster.Nodes[PCGEx::H64A(Node.Adjacency[0])].Position - Node.Position).GetSafeNormal();
			const FVector B = (Node.Position - Cluster.Nodes[PCGEx::H64A(Node.Adjacency[1])].Position).GetSafeNormal();
			if (FVector::DotProduct(A, B) < Context->FixedDotThreshold)
			{
				NodeFixtures.Add(Node.NodeIndex);
			}
		}
	}

	// Find starting search points
	for (const PCGExCluster::FNode& Node : ClusterNodes)
	{
		if (!NodeFixtures.Contains(Node.NodeIndex)) { continue; }

		for (const uint64 AdjacencyHash : Node.Adjacency)
		{
			const int32 AdjacentNode = PCGEx::H64A(AdjacencyHash);
			if (NodeFixtures.Contains(AdjacentNode)) { continue; }
			Candidates.AddUnique(AdjacentNode);
		}
	}

	for (const int32 CandidateIndex : Candidates)
	{
		const PCGExCluster::FNode& Node = ClusterNodes[CandidateIndex];

		if (VisitedNodes.Contains(Node.NodeIndex)) { continue; }
		VisitedNodes.Add(Node.NodeIndex);

		if (NodeFixtures.Contains(Node.NodeIndex)) { continue; } // Skip

		PCGExCluster::FNodeChain* NewChain = new PCGExCluster::FNodeChain();

		int32 NextNodeIndex = Node.NodeIndex;
		int32 PrevNodeIndex = NodeFixtures.Contains(PCGEx::H64A(Node.Adjacency[0])) ? PCGEx::H64A(Node.Adjacency[0]) : PCGEx::H64A(Node.Adjacency[1]);

		NewChain->First = PrevNodeIndex;
		while (NextNodeIndex != -1)
		{
			const int32 CurrentNodeIndex = NextNodeIndex;
			VisitedNodes.Add(CurrentNodeIndex);

			const PCGExCluster::FNode& NextNode = ClusterNodes[CurrentNodeIndex];

			NewChain->Edges.Add(NextNode.GetEdgeIndex(PrevNodeIndex));

			if (NodeFixtures.Contains(NextNodeIndex))
			{
				NewChain->Last = NextNodeIndex;
				break;
			}

			NewChain->Nodes.Add(CurrentNodeIndex);

			NextNodeIndex = PCGEx::H64A(NextNode.Adjacency[0]) == PrevNodeIndex ? NextNode.Adjacency.Num() == 1 ? -1 : PCGEx::H64A(NextNode.Adjacency[1]) : PCGEx::H64A(NextNode.Adjacency[0]);
			PrevNodeIndex = CurrentNodeIndex;

			if (NextNodeIndex == -1) { NewChain->Last = PrevNodeIndex; }
		}

		bool bIsDeadEnd = (DeadEnds.Contains(NewChain->First) || DeadEnds.Contains(NewChain->Last));
		if (Settings->bOperateOnDeadEndsOnly && !bIsDeadEnd)
		{
			delete NewChain;
			continue;
		}

		if (NewChain)
		{
			for (const int32 Edge : NewChain->Edges) { Context->CurrentCluster->Edges[Edge].bValid = false; }
			for (const int32 NodeIndex : NewChain->Nodes) { GraphNodes[ClusterNodes[NodeIndex].PointIndex].bValid = false; }
		}

		if (bIsDeadEnd) { delete NewChain; }
		else { Context->Chains.Add(NewChain); }
	}

	NodeFixtures.Empty();
	VisitedNodes.Empty();
	Candidates.Empty();

	return true;
}

#undef LOCTEXT_NAMESPACE
