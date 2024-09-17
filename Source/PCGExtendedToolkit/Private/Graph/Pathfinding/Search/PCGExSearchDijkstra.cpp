// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Search/PCGExSearchDijkstra.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"
#include "Graph/Pathfinding/Search/PCGExScoredQueue.h"

void UPCGExSearchDijkstra::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
}

bool UPCGExSearchDijkstra::FindPath(
	const FVector& SeedPosition,
	const FPCGExNodeSelectionDetails* SeedSelection,
	const FVector& GoalPosition,
	const FPCGExNodeSelectionDetails* GoalSelection,
	PCGExHeuristics::THeuristicsHandler* Heuristics,
	TArray<int32>& OutPath, PCGExHeuristics::FLocalFeedbackHandler* LocalFeedback) const
{
	const TArray<PCGExCluster::FNode>& NodesRef = *Cluster->Nodes;
	const TArray<PCGExGraph::FIndexedEdge>& EdgesRef = *Cluster->Edges;

	const PCGExCluster::FNode& SeedNode = NodesRef[Cluster->FindClosestNode(SeedPosition, SeedSelection->PickingMethod, 1)];
	if (!SeedSelection->WithinDistance(Cluster->GetPos(SeedNode), SeedPosition)) { return false; }

	const PCGExCluster::FNode& GoalNode = NodesRef[Cluster->FindClosestNode(GoalPosition, GoalSelection->PickingMethod, 1)];
	if (!GoalSelection->WithinDistance(Cluster->GetPos(GoalNode), GoalPosition)) { return false; }

	if (SeedNode.NodeIndex == GoalNode.NodeIndex) { return false; }

	const int32 NumNodes = NodesRef.Num();

	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExSearchDijkstra::FindPath);


	// Basic Dijkstra implementation

	TBitArray<> Visited;
	Visited.Init(false, NumNodes);

	TArray<uint64> TravelStack;

	PCGExSearch::TScoredQueue* ScoredQueue = new PCGExSearch::TScoredQueue(
		NumNodes, SeedNode.NodeIndex, 0);

	TravelStack.SetNumUninitialized(NumNodes);
	for (int i = 0; i < NumNodes; ++i)
	{
		ScoredQueue->Scores[i] = -1;
		TravelStack[i] = PCGEx::NH64(-1, -1);
	}

	ScoredQueue->Scores[SeedNode.NodeIndex] = 0;

	int32 CurrentNodeIndex;
	double CurrentScore;
	while (ScoredQueue->Dequeue(CurrentNodeIndex, CurrentScore))
	{
		if (CurrentNodeIndex == GoalNode.NodeIndex) { break; } // Exit early

		const PCGExCluster::FNode& Current = NodesRef[CurrentNodeIndex];

		if (Visited[CurrentNodeIndex]) { continue; }
		Visited[CurrentNodeIndex] = true;

		for (const uint64 AdjacencyHash : Current.Adjacency)
		{
			uint32 NeighborIndex;
			uint32 EdgeIndex;
			PCGEx::H64(AdjacencyHash, NeighborIndex, EdgeIndex);

			if (Visited[NeighborIndex]) { continue; }

			const PCGExCluster::FNode& AdjacentNode = NodesRef[NeighborIndex];
			const PCGExGraph::FIndexedEdge& Edge = EdgesRef[EdgeIndex];

			const double AltScore = CurrentScore + Heuristics->GetEdgeScore(Current, AdjacentNode, Edge, SeedNode, GoalNode, LocalFeedback, &TravelStack);
			const double PreviousScore = ScoredQueue->Scores[NeighborIndex];
			if (PreviousScore != -1 && AltScore >= PreviousScore) { continue; }

			ScoredQueue->Enqueue(NeighborIndex, AltScore);
			TravelStack[NeighborIndex] = PCGEx::NH64(CurrentNodeIndex, EdgeIndex);
		}
	}

	TArray<int32> Path;

	uint64 PathHash = TravelStack[GoalNode.NodeIndex];
	int32 PathNodeIndex;
	int32 PathEdgeIndex;
	PCGEx::NH64(PathHash, PathNodeIndex, PathEdgeIndex);

	if (PathNodeIndex != -1)
	{
		Path.Add(GoalNode.NodeIndex);
		if (PathNodeIndex != -1)
		{
			const PCGExGraph::FIndexedEdge& E = EdgesRef[PathEdgeIndex];
			Heuristics->FeedbackScore(GoalNode, E);
			if (LocalFeedback) { Heuristics->FeedbackScore(GoalNode, E); }
		}
		else
		{
			Heuristics->FeedbackPointScore(GoalNode);
			if (LocalFeedback) { Heuristics->FeedbackPointScore(GoalNode); }
		}
	}

	while (PathNodeIndex != -1)
	{
		const int32 CurrentIndex = PathNodeIndex;
		Path.Add(CurrentIndex);

		PathHash = TravelStack[PathNodeIndex];
		PCGEx::NH64(PathHash, PathNodeIndex, PathEdgeIndex);

		const PCGExCluster::FNode& N = NodesRef[CurrentIndex];
		if (PathNodeIndex != -1)
		{
			const PCGExGraph::FIndexedEdge& E = EdgesRef[PathEdgeIndex];
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
	TravelStack.Empty();
	PCGEX_DELETE(ScoredQueue)

	return true;
}
