// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Search/PCGExSearchDijkstra.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/PCGExPathfinding.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicOperation.h"
#include "Graph/Pathfinding/Search/PCGExScoredQueue.h"

bool UPCGExSearchDijkstra::FindPath(const PCGExCluster::FCluster* Cluster, const FVector& SeedPosition, const FVector& GoalPosition, const UPCGExHeuristicOperation* Heuristics, const FPCGExHeuristicModifiersSettings* Modifiers, TArray<int32>& OutPath)
{
	const PCGExCluster::FNode& SeedNode = Cluster->Nodes[Cluster->FindClosestNode(SeedPosition)];
	const PCGExCluster::FNode& GoalNode = Cluster->Nodes[Cluster->FindClosestNode(GoalPosition)];

	if (SeedNode.NodeIndex == GoalNode.NodeIndex) { return false; }

	const int32 NumNodes = Cluster->Nodes.Num();

	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathfinding::FindPath);


	// Basic Dijkstra implementation

	TSet<int32> Visited;
	TMap<int32, double> Scores;
	TMap<int32, int32> Previous;
	PCGExSearch::TScoredQueue<int32>* ScoredQueue = new PCGExSearch::TScoredQueue<int32>(SeedNode.NodeIndex, 0);

	Scores.Reserve(NumNodes);
	Scores.Add(SeedNode.NodeIndex, 0);

	int32 CurrentNodeIndex;
	double CurrentScore;
	while (ScoredQueue->Dequeue(CurrentNodeIndex, CurrentScore))
	{
		if (CurrentNodeIndex == GoalNode.NodeIndex) { break; } // Exit early

		const PCGExCluster::FNode& Current = Cluster->Nodes[CurrentNodeIndex];
		Visited.Add(CurrentNodeIndex);

		for (const int32 AdjacentNodeIndex : Current.AdjacentNodes)
		{
			if (Visited.Contains(AdjacentNodeIndex)) { continue; }

			const PCGExCluster::FNode& AdjacentNode = Cluster->Nodes[AdjacentNodeIndex];
			const PCGExGraph::FIndexedEdge& Edge = Cluster->GetEdgeFromNodeIndices(CurrentNodeIndex, AdjacentNodeIndex);

			const double ScoreMod = Modifiers->GetScore(AdjacentNode.PointIndex, Edge.PointIndex);
			const double AltScore = CurrentScore + Heuristics->GetEdgeScore(Current, AdjacentNode, Edge, SeedNode, GoalNode) + ScoreMod;

			const double* PreviousScore = Scores.Find(AdjacentNodeIndex);
			if (PreviousScore && AltScore > *PreviousScore) { continue; }

			ScoredQueue->SetScore(AdjacentNodeIndex, AltScore, !Visited.Contains(AdjacentNodeIndex));
			Scores.Add(AdjacentNodeIndex, AltScore);
			Previous.Add(AdjacentNodeIndex, CurrentNodeIndex);
		}
	}

	TArray<int32> Path;
	int32 PathIndex = GoalNode.NodeIndex;

	while (PathIndex != -1)
	{
		Path.Add(PathIndex);
		const int32* PathIndexPtr = Previous.Find(PathIndex);
		PathIndex = PathIndexPtr ? *PathIndexPtr : -1;
	}

	Algo::Reverse(Path);
	OutPath.Append(Path);

	Visited.Empty();
	Scores.Empty();
	Previous.Empty();
	PCGEX_DELETE(ScoredQueue)

	return true;
}
