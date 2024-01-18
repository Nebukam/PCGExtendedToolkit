// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Search/PCGExSearchDijkstra.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/PCGExPathfinding.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicOperation.h"

bool UPCGExSearchDijkstra::FindPath(const PCGExCluster::FCluster* Cluster, const FVector& SeedPosition, const FVector& GoalPosition, const UPCGExHeuristicOperation* Heuristics, const FPCGExHeuristicModifiersSettings* Modifiers, TArray<int32>& OutPath)
{
	const PCGExCluster::FNode& SeedNode = Cluster->Nodes[Cluster->FindClosestNode(SeedPosition)];
	const PCGExCluster::FNode& GoalNode = Cluster->Nodes[Cluster->FindClosestNode(GoalPosition)];

	if (SeedNode.NodeIndex == GoalNode.NodeIndex) { return false; }

	const int32 NumNodes = Cluster->Nodes.Num();

	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathfinding::FindPath);


	// Basic Dijkstra implementation TODO:Optimize

	TMap<int32, int32> Previous;
	TSet<int32> Visited;
	Visited.Reserve(NumNodes);

	TArray<double> Scores;
	TArray<double> FScore;

	Scores.SetNum(NumNodes);
	FScore.SetNum(NumNodes);
	TArray<int32> OpenList;

	for (int i = 0; i < NumNodes; i++)
	{
		Scores[i] = TNumericLimits<double>::Max();
		FScore[i] = TNumericLimits<double>::Max();
	}

	Scores[SeedNode.NodeIndex] = 0;
	OpenList.Add(SeedNode.NodeIndex);

	while (!OpenList.IsEmpty())
	{
		// the node in openSet having the lowest FScore value
		const PCGExCluster::FNode& Current = Cluster->Nodes[OpenList.Pop()]; // TODO: Sorted add, otherwise this won't work.
		Visited.Remove(Current.NodeIndex);

		const double CurrentScore = Scores[Current.NodeIndex];

		//Get current index neighbors
		for (const int32 AdjacentNodeIndex : Current.AdjacentNodes)
		{
			if (Visited.Contains(AdjacentNodeIndex)) { continue; }
			Visited.Add(AdjacentNodeIndex);

			const PCGExCluster::FNode& AdjacentNode = Cluster->Nodes[AdjacentNodeIndex];
			const PCGExGraph::FIndexedEdge& Edge = Cluster->GetEdgeFromNodeIndices(Current.NodeIndex, AdjacentNodeIndex);

			const double ScoreOffset = Modifiers->GetScore(AdjacentNode.PointIndex, Edge.EdgeIndex);
			const double AltScore = CurrentScore + Heuristics->GetEdgeScore(Current, AdjacentNode, Edge, SeedNode, GoalNode) + ScoreOffset;

			if (AltScore >= Scores[AdjacentNode.NodeIndex]) { continue; }

			Scores[AdjacentNode.NodeIndex] = AltScore;
			Previous.Add(AdjacentNode.NodeIndex, Current.NodeIndex);
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

	return true;
}
