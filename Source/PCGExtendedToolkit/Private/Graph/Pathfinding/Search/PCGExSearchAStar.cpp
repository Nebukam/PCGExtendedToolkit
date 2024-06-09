// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Search/PCGExSearchAStar.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"
#include "Graph/Pathfinding/Search/PCGExScoredQueue.h"

bool UPCGExSearchAStar::FindPath(
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

	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExSearchAStar::FindPath);

	// Basic A* implementation TODO:Optimize

	TSet<int32> Visited;
	TArray<uint64> Previous;
	TArray<double> GScore;

	GScore.SetNum(NumNodes);
	Previous.SetNum(NumNodes);
	for (int i = 0; i < NumNodes; i++)
	{
		GScore[i] = -1;
		Previous[i] = PCGEx::NH64(-1, -1);
	}

	double MinGScore = TNumericLimits<double>::Max();
	double MaxGScore = TNumericLimits<double>::Min();

	for (int i = 0; i < Cluster->Nodes.Num(); i++)
	{
		const double GS = Heuristics->GetGlobalScore(Cluster->Nodes[i], SeedNode, GoalNode);
		MinGScore = FMath::Min(MinGScore, GS);
		MaxGScore = FMath::Max(MaxGScore, GS);
	}

	PCGExSearch::TScoredQueue* ScoredQueue = new PCGExSearch::TScoredQueue(
		NumNodes, SeedNode.NodeIndex, Heuristics->GetGlobalScore(SeedNode, SeedNode, GoalNode));
	GScore[SeedNode.NodeIndex] = 0;

	bool bSuccess = false;

	int32 CurrentNodeIndex;
	double CurrentFScore;
	while (ScoredQueue->Dequeue(CurrentNodeIndex, CurrentFScore))
	{
		if (CurrentNodeIndex == GoalNode.NodeIndex) { break; } // Exit early

		const double CurrentGScore = GScore[CurrentNodeIndex];
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

			const double EScore = Heuristics->GetEdgeScore(Current, AdjacentNode, Edge, SeedNode, GoalNode, LocalFeedback);
			const double TentativeGScore = CurrentGScore + EScore;

			const double PreviousGScore = GScore[NeighborIndex];
			if (PreviousGScore != -1 && TentativeGScore >= PreviousGScore) { continue; }

			Previous[NeighborIndex] = PCGEx::NH64(CurrentNodeIndex, EdgeIndex);
			GScore[NeighborIndex] = TentativeGScore;

			const double GS = PCGExMath::Remap(Heuristics->GetGlobalScore(AdjacentNode, SeedNode, GoalNode), MinGScore, MaxGScore, 0, 1);
			const double FScore = TentativeGScore + GS * Heuristics->ReferenceWeight; //TODO: Need to weight this properly

			ScoredQueue->Enqueue(NeighborIndex, FScore);
		}
	}

	uint64 PathHash = Previous[GoalNode.NodeIndex];
	int32 PathNodeIndex;
	int32 PathEdgeIndex;
	PCGEx::NH64(PathHash, PathNodeIndex, PathEdgeIndex);

	if (PathNodeIndex != -1)
	{
		bSuccess = true;
		TArray<int32> Path;

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
	}

	PCGEX_DELETE(ScoredQueue)
	Previous.Empty();
	GScore.Empty();

	return bSuccess;
}
