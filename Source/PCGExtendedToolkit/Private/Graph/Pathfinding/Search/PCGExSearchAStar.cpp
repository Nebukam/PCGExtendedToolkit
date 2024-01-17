// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Search/PCGExSearchAStar.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/PCGExPathfinding.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicOperation.h"

bool UPCGExSearchAStar::FindPath(const PCGExCluster::FCluster* Cluster, const FVector& SeedPosition, const FVector& GoalPosition, const UPCGExHeuristicOperation* Heuristics, const FPCGExHeuristicModifiersSettings* Modifiers, TArray<int32>& OutPath)
{
	const PCGExCluster::FNode& SeedNode = Cluster->Nodes[Cluster->FindClosestNode(SeedPosition)];
	const PCGExCluster::FNode& GoalNode = Cluster->Nodes[Cluster->FindClosestNode(GoalPosition)];

	if (SeedNode.NodeIndex == GoalNode.NodeIndex) { return false; }

	const int32 NumNodes = Cluster->Nodes.Num();

	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathfinding::FindPath);


	// Basic A* implementation TODO:Optimize

	TMap<int32, int32> Previous;
	TSet<int32> Visited;
	Visited.Reserve(NumNodes);

	TArray<double> GScore;
	TArray<double> FScore;

	GScore.SetNum(NumNodes);
	FScore.SetNum(NumNodes);

	for (int i = 0; i < NumNodes; i++)
	{
		GScore[i] = TNumericLimits<double>::Max();
		FScore[i] = TNumericLimits<double>::Max();
	}

	GScore[SeedNode.NodeIndex] = 0;
	FScore[SeedNode.NodeIndex] = Heuristics->ComputeFScore(SeedNode, SeedNode, GoalNode);

	TArray<int32> OpenList;
	OpenList.Add(SeedNode.NodeIndex);

	bool bSuccess = false;

	while (!OpenList.IsEmpty())
	{
		const PCGExCluster::FNode& Current = Cluster->Nodes[OpenList.Pop()];

		if (Current.NodeIndex == GoalNode.NodeIndex)
		{
			bSuccess = true;
			TArray<int32> Path;
			int32 PathIndex = Current.NodeIndex;

			//WARNING: This will loop forever if we introduce negative weights.
			while (PathIndex != -1)
			{
				Path.Add(PathIndex);
				const int32* PathIndexPtr = Previous.Find(PathIndex);
				PathIndex = PathIndexPtr ? *PathIndexPtr : -1;
			}

			Algo::Reverse(Path);
			OutPath.Append(Path);

			break;
		}

		const double CurrentGScore = GScore[Current.NodeIndex];

		for (const int32 AdjacentNodeIndex : Current.AdjacentNodes)
		{
			const PCGExCluster::FNode& AdjacentNode = Cluster->Nodes[AdjacentNodeIndex];
			const PCGExGraph::FIndexedEdge& Edge = Cluster->GetEdgeFromNodeIndices(Current.NodeIndex, AdjacentNodeIndex);

			const double ScoreOffset = Modifiers->GetScore(AdjacentNode.PointIndex, Edge.EdgeIndex);
			const double TentativeGScore = CurrentGScore + Heuristics->ComputeDScore(Current, AdjacentNode, Edge, SeedNode, GoalNode) + ScoreOffset;

			if (TentativeGScore >= GScore[AdjacentNode.NodeIndex]) { continue; }

			Previous.Add(AdjacentNode.NodeIndex, Current.NodeIndex);
			GScore[AdjacentNode.NodeIndex] = TentativeGScore;
			FScore[AdjacentNode.NodeIndex] = TentativeGScore + Heuristics->ComputeFScore(AdjacentNode, SeedNode, GoalNode) + ScoreOffset;

			if (!Visited.Contains(AdjacentNode.NodeIndex))
			{
				PCGEX_SORTED_ADD(OpenList, AdjacentNode.NodeIndex, TentativeGScore < GScore[i])
				Visited.Add(AdjacentNode.NodeIndex);
			}
		}
	}

	return bSuccess;
}
