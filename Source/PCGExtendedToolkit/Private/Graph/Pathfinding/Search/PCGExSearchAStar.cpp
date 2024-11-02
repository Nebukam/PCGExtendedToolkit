// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Search/PCGExSearchAStar.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"
#include "Graph/Pathfinding/Search/PCGExScoredQueue.h"

bool UPCGExSearchAStar::ResolveQuery(
	const TSharedPtr<PCGExPathfinding::FPathQuery>& InQuery,
	const TSharedPtr<PCGExHeuristics::FHeuristicsHandler>& Heuristics,
	const TSharedPtr<PCGExHeuristics::FLocalFeedbackHandler>& LocalFeedback) const
{
	check(InQuery->PickResolution == PCGExPathfinding::EQueryPickResolution::Success)

	const TArray<PCGExCluster::FNode>& NodesRef = *Cluster->Nodes;
	const TArray<PCGExGraph::FIndexedEdge>& EdgesRef = *Cluster->Edges;

	const PCGExCluster::FNode& SeedNode = *InQuery->Seed.Node;
	const PCGExCluster::FNode& GoalNode = *InQuery->Goal.Node;

	const int32 NumNodes = NodesRef.Num();

	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExSearchAStar::FindPath);

	TBitArray<> Visited;
	Visited.Init(false, NumNodes);

	TArray<uint64> TravelStack;
	TArray<double> GScore;

	GScore.Init(-1, NumNodes);
	TravelStack.Init(PCGEx::NH64(-1, -1), NumNodes);

	double MinGScore = MAX_dbl;
	double MaxGScore = MIN_dbl;

	// TODO : Global score can be computed & cached once 
	for (int i = 0; i < NodesRef.Num(); i++)
	{
		const double GS = Heuristics->GetGlobalScore(NodesRef[i], SeedNode, GoalNode);
		MinGScore = FMath::Min(MinGScore, GS);
		MaxGScore = FMath::Max(MaxGScore, GS);
	}

	const TUniquePtr<PCGExSearch::TScoredQueue> ScoredQueue = MakeUnique<PCGExSearch::TScoredQueue>(
		NumNodes, SeedNode.NodeIndex, Heuristics->GetGlobalScore(SeedNode, SeedNode, GoalNode));

	GScore[SeedNode.NodeIndex] = 0;

	const PCGExHeuristics::FLocalFeedbackHandler* Feedback = LocalFeedback.Get();

	int32 VisitedNum = 0;
	int32 CurrentNodeIndex;
	double CurrentFScore;
	while (ScoredQueue->Dequeue(CurrentNodeIndex, CurrentFScore))
	{
		if (CurrentNodeIndex == GoalNode.NodeIndex) { break; } // Exit early

		const double CurrentGScore = GScore[CurrentNodeIndex];
		const PCGExCluster::FNode& Current = NodesRef[CurrentNodeIndex];

		if (Visited[CurrentNodeIndex]) { continue; }
		Visited[CurrentNodeIndex] = true;
		VisitedNum++;

		for (const uint64 AdjacencyHash : Current.Adjacency)
		{
			uint32 NeighborIndex;
			uint32 EdgeIndex;
			PCGEx::H64(AdjacencyHash, NeighborIndex, EdgeIndex);

			if (Visited[NeighborIndex]) { continue; }

			const PCGExCluster::FNode& AdjacentNode = NodesRef[NeighborIndex];
			const PCGExGraph::FIndexedEdge& Edge = EdgesRef[EdgeIndex];

			const double EScore = Heuristics->GetEdgeScore(Current, AdjacentNode, Edge, SeedNode, GoalNode, Feedback, &TravelStack);
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

	bool bSuccess = false;

	int32 PathNodeIndex = PCGEx::NH64A(TravelStack[GoalNode.NodeIndex]);
	int32 PathEdgeIndex = -1;

	if (PathNodeIndex != -1)
	{
		bSuccess = true;
		InQuery->Reserve(VisitedNum);

		InQuery->AddPathNode(GoalNode.NodeIndex);
		
		while (PathNodeIndex != -1)
		{
			const int32 CurrentIndex = PathNodeIndex;
			PCGEx::NH64(TravelStack[CurrentIndex], PathNodeIndex, PathEdgeIndex);

			InQuery->AddPathNode(CurrentIndex, PathEdgeIndex);
		}

	}

	TravelStack.Empty();
	GScore.Empty();

	return bSuccess;
}
