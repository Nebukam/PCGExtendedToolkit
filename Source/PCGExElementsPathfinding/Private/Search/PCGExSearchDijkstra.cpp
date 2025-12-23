// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Search/PCGExSearchDijkstra.h"


#include "PCGExHeuristicsHandler.h"
#include "Clusters/PCGExCluster.h"
#include "Containers/PCGExHashLookup.h"
#include "Core/PCGExPathfinding.h"
#include "Core/PCGExPathQuery.h"
#include "Core/PCGExSearchAllocations.h"
#include "Utils/PCGExScoredQueue.h"

bool FPCGExSearchOperationDijkstra::ResolveQuery(
	const TSharedPtr<PCGExPathfinding::FPathQuery>& InQuery,
	const TSharedPtr<PCGExPathfinding::FSearchAllocations>& Allocations,
	const TSharedPtr<PCGExHeuristics::FHandler>& Heuristics,
	const TSharedPtr<PCGExHeuristics::FLocalFeedbackHandler>& LocalFeedback) const
{
	TSharedPtr<PCGExPathfinding::FSearchAllocations> LocalAllocations = Allocations;
	if (!LocalAllocations) { LocalAllocations = NewAllocations(); }
	else { LocalAllocations->Reset(); }

	const TArray<PCGExClusters::FNode>& NodesRef = *Cluster->Nodes;
	const TArray<PCGExGraphs::FEdge>& EdgesRef = *Cluster->Edges;

	const PCGExClusters::FNode& SeedNode = *InQuery->Seed.Node;
	const PCGExClusters::FNode& GoalNode = *InQuery->Goal.Node;

	const int32 NumNodes = NodesRef.Num();

	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExSearchDijkstra::FindPath);

	// Basic Dijkstra implementation

	TBitArray<> Visited;
	Visited.Init(false, NumNodes);

	// TODO : Use local allocations for the hash lookup
	const TSharedPtr<PCGEx::FHashLookup> TravelStack = PCGEx::NewHashLookup<PCGEx::FHashLookupArray>(PCGEx::NH64(-1, -1), NumNodes);

	const TUniquePtr<PCGEx::FScoredQueue> ScoredQueue = MakeUnique<PCGEx::FScoredQueue>(NumNodes);
	ScoredQueue->Enqueue(SeedNode.Index, 0);

	const PCGExHeuristics::FLocalFeedbackHandler* Feedback = LocalFeedback.Get();

	int32 VisitedNum = 0;
	int32 CurrentNodeIndex;
	double CurrentScore;
	while (ScoredQueue->Dequeue(CurrentNodeIndex, CurrentScore))
	{
		if (bEarlyExit && CurrentNodeIndex == GoalNode.Index) { break; } // Exit early

		const PCGExClusters::FNode& Current = NodesRef[CurrentNodeIndex];

		if (Visited[CurrentNodeIndex]) { continue; }
		Visited[CurrentNodeIndex] = true;
		VisitedNum++;

		for (const PCGExGraphs::FLink Lk : Current.Links)
		{
			const uint32 NeighborIndex = Lk.Node;
			const uint32 EdgeIndex = Lk.Edge;

			if (Visited[NeighborIndex]) { continue; }

			const PCGExClusters::FNode& AdjacentNode = NodesRef[NeighborIndex];
			const PCGExGraphs::FEdge& Edge = EdgesRef[EdgeIndex];

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
