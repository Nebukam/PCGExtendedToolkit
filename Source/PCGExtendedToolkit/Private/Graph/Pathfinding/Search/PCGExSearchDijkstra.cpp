// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Search/PCGExSearchDijkstra.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"
#include "Graph/Pathfinding/Search/PCGExScoredQueue.h"

bool UPCGExSearchDijkstra::FindPath(
	const FVector& SeedPosition,
	const FPCGExNodeSelectionSettings* SeedSelection,
	const FVector& GoalPosition,
	const FPCGExNodeSelectionSettings* GoalSelection,
	PCGExHeuristics::THeuristicsHandler* Heuristics,
	TArray<int32>& OutPath, PCGExHeuristics::FLocalFeedbackHandler* LocalFeedback)
{
	const PCGExCluster::FNode& SeedNode = Cluster->Nodes[Cluster->FindClosestNode(SeedPosition, SeedSelection->PickingMethod, 1)];
	if (!SeedSelection->WithinDistance(SeedNode.Position, SeedPosition)) { return false; }

	const PCGExCluster::FNode& GoalNode = Cluster->Nodes[Cluster->FindClosestNode(GoalPosition, GoalSelection->PickingMethod, 1)];
	if (!GoalSelection->WithinDistance(GoalNode.Position, GoalPosition)) { return false; }

	if (SeedNode.NodeIndex == GoalNode.NodeIndex) { return false; }

	const int32 NumNodes = Cluster->Nodes.Num();

	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExSearchDijkstra::FindPath);


	// Basic Dijkstra implementation

	TSet<int32> Visited;
	TArray<uint64> Previous;

	PCGExSearch::TScoredQueue* ScoredQueue = new PCGExSearch::TScoredQueue(
		NumNodes, SeedNode.NodeIndex, 0);

	Previous.SetNumUninitialized(NumNodes);
	for (int i = 0; i < NumNodes; i++)
	{
		ScoredQueue->Scores[i] = -1;
		Previous[i] = PCGEx::NH64(-1,-1);
	}

	ScoredQueue->Scores[SeedNode.NodeIndex] = 0;

	int32 CurrentNodeIndex;
	double CurrentScore;
	while (ScoredQueue->Dequeue(CurrentNodeIndex, CurrentScore))
	{
		if (CurrentNodeIndex == GoalNode.NodeIndex) { break; } // Exit early

		const PCGExCluster::FNode& Current = Cluster->Nodes[CurrentNodeIndex];
		Visited.Add(CurrentNodeIndex);

		for (const uint64 AdjacencyHash : Current.Adjacency)
		{
			uint32 NeighborIndex;
			uint32 EdgeIndex;
			PCGEx::H64(AdjacencyHash, NeighborIndex, EdgeIndex);

			if (Visited.Contains(NeighborIndex)) { continue; }

			const PCGExCluster::FNode& AdjacentNode = Cluster->Nodes[NeighborIndex];
			const PCGExGraph::FIndexedEdge& Edge = Cluster->Edges[EdgeIndex];

			const double AltScore = CurrentScore + Heuristics->GetEdgeScore(Current, AdjacentNode, Edge, SeedNode, GoalNode, LocalFeedback);
			const double PreviousScore = ScoredQueue->Scores[NeighborIndex];
			if (PreviousScore != -1 && AltScore > PreviousScore) { continue; }

			ScoredQueue->Enqueue(NeighborIndex, AltScore);
			Previous[NeighborIndex] = PCGEx::NH64(CurrentNodeIndex, EdgeIndex);
		}
	}

	TArray<int32> Path;

	uint64 PathHash = Previous[GoalNode.NodeIndex];
	int32 PathNodeIndex;
	int32 PathEdgeIndex;
	PCGEx::NH64(PathHash, PathNodeIndex, PathEdgeIndex);
	
	while (PathNodeIndex != -1)
	{
		const int32 CurrentIndex = PathNodeIndex;
		Path.Add(CurrentIndex);
		
		PathHash = Previous[PathNodeIndex];
		PCGEx::NH64(PathHash, PathNodeIndex, PathEdgeIndex);

		const PCGExCluster::FNode& N = Cluster->Nodes[CurrentIndex];
		if (PathNodeIndex != -1)
		{
			const PCGExGraph::FIndexedEdge& E = Cluster->Edges[PathEdgeIndex];
			Heuristics->FeedbackScore(N, E);
			if (LocalFeedback) { Heuristics->FeedbackScore(N, E); }
		}
		else
		{
			Heuristics->FeedbackPointScore(N);
			if (LocalFeedback) { Heuristics->FeedbackPointScore(N); }
		}
	}


	Algo::Reverse(Path);
	OutPath.Append(Path);

	Visited.Empty();
	Previous.Empty();
	PCGEX_DELETE(ScoredQueue)

	return true;
}
