// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Search/PCGExSearchDijkstra.h"


#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"
#include "Graph/Pathfinding/Search/PCGExScoredQueue.h"

void UPCGExSearchDijkstra::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
}

bool UPCGExSearchDijkstra::ResolveQuery(
	const TSharedPtr<PCGExPathfinding::FPathQuery>& InQuery,
	const TSharedPtr<PCGExHeuristics::FHeuristicsHandler>& Heuristics,
	const TSharedPtr<PCGExHeuristics::FLocalFeedbackHandler>& LocalFeedback) const
{
	const TArray<PCGExCluster::FNode>& NodesRef = *Cluster->Nodes;
	const TArray<PCGExGraph::FEdge>& EdgesRef = *Cluster->Edges;

	const PCGExCluster::FNode& SeedNode = *InQuery->Seed.Node;
	const PCGExCluster::FNode& GoalNode = *InQuery->Goal.Node;

	const int32 NumNodes = NodesRef.Num();

	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExSearchDijkstra::FindPath);

	// Basic Dijkstra implementation

	TBitArray<> Visited;
	Visited.Init(false, NumNodes);

	const TSharedPtr<PCGEx::FHashLookup> TravelStack = PCGEx::NewHashLookup<PCGEx::FArrayHashLookup>(PCGEx::NH64(-1, -1), NumNodes);

	const TUniquePtr<PCGExSearch::FScoredQueue> ScoredQueue = MakeUnique<PCGExSearch::FScoredQueue>(
		NumNodes, SeedNode.Index, 0);

	const PCGExHeuristics::FLocalFeedbackHandler* Feedback = LocalFeedback.Get();

	int32 VisitedNum = 0;
	int32 CurrentNodeIndex;
	double CurrentScore;
	while (ScoredQueue->Dequeue(CurrentNodeIndex, CurrentScore))
	{
		if (CurrentNodeIndex == GoalNode.Index && bEarlyExit) { break; } // Exit early

		const PCGExCluster::FNode& Current = NodesRef[CurrentNodeIndex];

		if (Visited[CurrentNodeIndex]) { continue; }
		Visited[CurrentNodeIndex] = true;
		VisitedNum++;

		for (const PCGExGraph::FLink Lk : Current.Links)
		{
			const uint32 NeighborIndex = Lk.Node;
			const uint32 EdgeIndex = Lk.Edge;

			if (Visited[NeighborIndex]) { continue; }

			const PCGExCluster::FNode& AdjacentNode = NodesRef[NeighborIndex];
			const PCGExGraph::FEdge& Edge = EdgesRef[EdgeIndex];

			const double AltScore = CurrentScore + Heuristics->GetEdgeScore(Current, AdjacentNode, Edge, SeedNode, GoalNode, Feedback, TravelStack);
			if (ScoredQueue->Enqueue(NeighborIndex, AltScore))
			{
				TravelStack->Set(NeighborIndex, PCGEx::NH64(CurrentNodeIndex, EdgeIndex));
			}
		}
	}

	bool bSuccess = false;

	int32 PathNodeIndex = PCGEx::NH64A(TravelStack->Get(GoalNode.Index));
	int32 PathEdgeIndex = -1;

	if (PathNodeIndex != -1)
	{
		bSuccess = true;
		//InQuery->Reserve(VisitedNum);

		InQuery->AddPathNode(GoalNode.Index);

		while (PathNodeIndex != -1)
		{
			const int32 CurrentIndex = PathNodeIndex;
			PCGEx::NH64(TravelStack->Get(CurrentIndex), PathNodeIndex, PathEdgeIndex);

			InQuery->AddPathNode(CurrentIndex, PathEdgeIndex);
		}
	}

	return bSuccess;
}
