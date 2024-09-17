// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Search/PCGExSearchAStar.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"
#include "Graph/Pathfinding/Search/PCGExScoredQueue.h"

bool UPCGExSearchAStar::FindPath(
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

	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExSearchAStar::FindPath);

	// Basic A* implementation TODO:Optimize

	TBitArray<> Visited;
	Visited.Init(false, NumNodes);

	TArray<uint64> TravelStack;
	TArray<double> GScore;

	GScore.SetNum(NumNodes);
	TravelStack.SetNum(NumNodes);
	for (int i = 0; i < NumNodes; ++i)
	{
		GScore[i] = -1;
		TravelStack[i] = PCGEx::NH64(-1, -1);
	}

	double MinGScore = TNumericLimits<double>::Max();
	double MaxGScore = TNumericLimits<double>::Min();

	for (int i = 0; i < NodesRef.Num(); ++i)
	{
		const double GS = Heuristics->GetGlobalScore(NodesRef[i], SeedNode, GoalNode);
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

			const double EScore = Heuristics->GetEdgeScore(Current, AdjacentNode, Edge, SeedNode, GoalNode, LocalFeedback, &TravelStack);
			const double TentativeGScore = CurrentGScore + EScore;

			const double PreviousGScore = GScore[NeighborIndex];
			if (PreviousGScore != -1 && TentativeGScore >= PreviousGScore) { continue; }

			TravelStack[NeighborIndex] = PCGEx::NH64(CurrentNodeIndex, EdgeIndex);
			GScore[NeighborIndex] = TentativeGScore;

			const double GS = PCGExMath::Remap(Heuristics->GetGlobalScore(AdjacentNode, SeedNode, GoalNode), MinGScore, MaxGScore, 0, 1);
			const double FScore = TentativeGScore + GS * Heuristics->ReferenceWeight; //TODO: Need to weight this properly

			ScoredQueue->Enqueue(NeighborIndex, FScore);
		}
	}

	uint64 PathHash = TravelStack[GoalNode.NodeIndex];
	int32 PathNodeIndex;
	int32 PathEdgeIndex;
	PCGEx::NH64(PathHash, PathNodeIndex, PathEdgeIndex);

	TArray<int32> Path;

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

	if (PathNodeIndex != -1)
	{
		bSuccess = true;

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
	}

	PCGEX_DELETE(ScoredQueue)
	TravelStack.Empty();
	GScore.Empty();

	return bSuccess;
}
