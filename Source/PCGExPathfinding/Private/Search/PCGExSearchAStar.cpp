// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Search/PCGExSearchAStar.h"

#include "PCGExHeuristicsHandler.h"
#include "Clusters/PCGExCluster.h"
#include "Containers/PCGExHashLookup.h"
#include "Core/PCGExPathfinding.h"
#include "Core/PCGExPathQuery.h"
#include "Core/PCGExSearchAllocations.h"
#include "Utils/PCGExScoredQueue.h"

bool FPCGExSearchOperationAStar::ResolveQuery(
	const TSharedPtr<PCGExPathfinding::FPathQuery>& InQuery,
	const TSharedPtr<PCGExPathfinding::FSearchAllocations>& Allocations,
	const TSharedPtr<PCGExHeuristics::FHandler>& Heuristics,
	const TSharedPtr<PCGExHeuristics::FLocalFeedbackHandler>& LocalFeedback) const
{
	check(InQuery->PickResolution == PCGExPathfinding::EQueryPickResolution::Success)

	TSharedPtr<PCGExPathfinding::FSearchAllocations> LocalAllocations = Allocations;
	if (!LocalAllocations) { LocalAllocations = NewAllocations(); }
	else { LocalAllocations->Reset(); }

	const TArray<PCGExClusters::FNode>& NodesRef = *Cluster->Nodes;
	const TArray<PCGExGraphs::FEdge>& EdgesRef = *Cluster->Edges;

	const PCGExClusters::FNode& SeedNode = *InQuery->Seed.Node;
	const PCGExClusters::FNode& GoalNode = *InQuery->Goal.Node;

	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExSearchAStar::FindPath);

	TBitArray<>& Visited = LocalAllocations->Visited;
	TArray<double>& GScore = LocalAllocations->GScore;
	const TSharedPtr<PCGEx::FHashLookup> TravelStack = LocalAllocations->TravelStack;
	const TSharedPtr<PCGEx::FScoredQueue> ScoredQueue = LocalAllocations->ScoredQueue;
	ScoredQueue->Enqueue(SeedNode.Index, Heuristics->GetGlobalScore(SeedNode, SeedNode, GoalNode));

	GScore[SeedNode.Index] = 0;

	const PCGExHeuristics::FLocalFeedbackHandler* Feedback = LocalFeedback.Get();

	int32 VisitedNum = 0;
	int32 CurrentNodeIndex;
	double CurrentFScore;
	while (ScoredQueue->Dequeue(CurrentNodeIndex, CurrentFScore))
	{
		if (bEarlyExit && CurrentNodeIndex == GoalNode.Index) { break; } // Exit early

		const double CurrentGScore = GScore[CurrentNodeIndex];
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

			const double EScore = Heuristics->GetEdgeScore(Current, AdjacentNode, Edge, SeedNode, GoalNode, Feedback, TravelStack);
			const double TentativeGScore = CurrentGScore + EScore;

			const double PreviousGScore = GScore[NeighborIndex];
			if (PreviousGScore != -1 && TentativeGScore >= PreviousGScore) { continue; }

			TravelStack->Set(NeighborIndex, PCGEx::NH64(CurrentNodeIndex, EdgeIndex));
			GScore[NeighborIndex] = TentativeGScore;

			const double GS = Heuristics->GetGlobalScore(AdjacentNode, SeedNode, GoalNode, Feedback);
			const double FScore = TentativeGScore + GS * Heuristics->ReferenceWeight;

			ScoredQueue->Enqueue(NeighborIndex, FScore);
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

TSharedPtr<PCGExPathfinding::FSearchAllocations> FPCGExSearchOperationAStar::NewAllocations() const
{
	TSharedPtr<PCGExPathfinding::FSearchAllocations> Allocations = FPCGExSearchOperation::NewAllocations();
	Allocations->GScore.Init(-1, Cluster->Nodes->Num());
	return Allocations;
}
