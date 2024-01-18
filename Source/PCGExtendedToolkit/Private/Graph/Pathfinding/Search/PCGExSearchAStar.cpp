// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Search/PCGExSearchAStar.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/PCGExPathfinding.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicOperation.h"
#include "Graph/Pathfinding/Search/PCGExScoredQueue.h"

bool UPCGExSearchAStar::FindPath(const PCGExCluster::FCluster* Cluster, const FVector& SeedPosition, const FVector& GoalPosition, const UPCGExHeuristicOperation* Heuristics, const FPCGExHeuristicModifiersSettings* Modifiers, TArray<int32>& OutPath)
{
	const PCGExCluster::FNode& SeedNode = Cluster->Nodes[Cluster->FindClosestNode(SeedPosition)];
	const PCGExCluster::FNode& GoalNode = Cluster->Nodes[Cluster->FindClosestNode(GoalPosition)];

	if (SeedNode.NodeIndex == GoalNode.NodeIndex) { return false; }

	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathfinding::FindPath);

	// Basic A* implementation TODO:Optimize

	TSet<int32> Visited;
	TMap<int32, int32> Previous;
	TMap<int32, double> GScore;

	PCGExSearch::TScoredQueue<int32>* ScoredQueue = new PCGExSearch::TScoredQueue<int32>(
		SeedNode.NodeIndex, Heuristics->GetGlobalScore(SeedNode, SeedNode, GoalNode));

	GScore.Add(SeedNode.NodeIndex, 0);

	bool bSuccess = false;

	int32 CurrentNodeIndex;
	double CurrentGScore;
	while (ScoredQueue->Dequeue(CurrentNodeIndex, CurrentGScore))
	{
		const PCGExCluster::FNode& Current = Cluster->Nodes[CurrentNodeIndex];
		Visited.Add(CurrentNodeIndex);

		if (Current.NodeIndex == GoalNode.NodeIndex && bExitEarly) { break; }

		for (const int32 AdjacentIndex : Current.AdjacentNodes)
		{
			const PCGExCluster::FNode& AdjacentNode = Cluster->Nodes[AdjacentIndex];
			const PCGExGraph::FIndexedEdge& Edge = Cluster->GetEdgeFromNodeIndices(CurrentNodeIndex, AdjacentIndex);

			const double ScoreMod = Modifiers->GetScore(AdjacentNode.PointIndex, Edge.EdgeIndex);
			const double TentativeGScore = CurrentGScore + Heuristics->GetEdgeScore(Current, AdjacentNode, Edge, SeedNode, GoalNode) + ScoreMod;

			if (const double* GScorePtr = GScore.Find(AdjacentIndex);
				GScorePtr && TentativeGScore >= *GScorePtr) { continue; }

			Previous.Add(AdjacentIndex, CurrentNodeIndex);
			GScore.Add(AdjacentIndex, TentativeGScore);
			const double FScore = TentativeGScore + Heuristics->GetGlobalScore(AdjacentNode, SeedNode, GoalNode);

			ScoredQueue->SetScore(AdjacentIndex, FScore, !Visited.Contains(AdjacentIndex));
		}
	}

	if (Previous.Contains(GoalNode.NodeIndex))
	{
		bSuccess = true;

		TArray<int32> Path;
		int32 PreviousIndex = GoalNode.NodeIndex;

		while (PreviousIndex != -1)
		{
			Path.Add(PreviousIndex);
			const int32* PathIndexPtr = Previous.Find(PreviousIndex);
			PreviousIndex = PathIndexPtr ? *PathIndexPtr : -1;
		}

		Algo::Reverse(Path);
		OutPath.Append(Path);
	}

	PCGEX_DELETE(ScoredQueue)
	Previous.Empty();
	GScore.Empty();

	return bSuccess;
}
