// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Search/PCGExSearchBidirectional.h"

#include "PCGExHeuristicsHandler.h"
#include "Clusters/PCGExCluster.h"
#include "Containers/PCGExHashLookup.h"
#include "Core/PCGExPathfinding.h"
#include "Core/PCGExPathQuery.h"
#include "Core/PCGExSearchAllocations.h"
#include "Utils/PCGExScoredQueue.h"

namespace PCGExPathfinding
{
	void FBidirectionalSearchAllocations::Init(const PCGExClusters::FCluster* InCluster)
	{
		FSearchAllocations::Init(InCluster);

		GScore.Init(-1, NumNodes);
		VisitedBackward.Init(false, NumNodes);
		GScoreBackward.Init(-1, NumNodes);
		TravelStackBackward = PCGEx::NewHashLookup<PCGEx::FHashLookupArray>(PCGEx::NH64(-1, -1), NumNodes);
		ScoredQueueBackward = MakeShared<PCGEx::FScoredQueue>(NumNodes);
	}

	void FBidirectionalSearchAllocations::Reset()
	{
		FSearchAllocations::Reset();

		for (int i = 0; i < NumNodes; i++) { GScore[i] = -1; }

		const int32 NumVisitedNodes = Visited.Num();
		for (int i = 0; i < NumVisitedNodes; i++)
		{
			VisitedBackward[i] = false;
			GScoreBackward[i] = -1;
		}

		TravelStackBackward->Reset();
		ScoredQueueBackward->Reset();
	}
}

bool FPCGExSearchOperationBidirectional::ResolveQuery(
	const TSharedPtr<PCGExPathfinding::FPathQuery>& InQuery,
	const TSharedPtr<PCGExPathfinding::FSearchAllocations>& Allocations,
	const TSharedPtr<PCGExHeuristics::FHandler>& Heuristics,
	const TSharedPtr<PCGExHeuristics::FLocalFeedbackHandler>& LocalFeedback) const
{
	check(InQuery->PickResolution == PCGExPathfinding::EQueryPickResolution::Success)

	TSharedPtr<PCGExPathfinding::FBidirectionalSearchAllocations> LocalAllocations;
	if (Allocations)
	{
		LocalAllocations = StaticCastSharedPtr<PCGExPathfinding::FBidirectionalSearchAllocations>(Allocations);
		LocalAllocations->Reset();
	}
	else
	{
		LocalAllocations = StaticCastSharedPtr<PCGExPathfinding::FBidirectionalSearchAllocations>(NewAllocations());
	}

	const TArray<PCGExClusters::FNode>& NodesRef = *Cluster->Nodes;
	const TArray<PCGExGraphs::FEdge>& EdgesRef = *Cluster->Edges;

	const PCGExClusters::FNode& SeedNode = *InQuery->Seed.Node;
	const PCGExClusters::FNode& GoalNode = *InQuery->Goal.Node;

	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSearchOperationBidirectional::FindPath);

	// Forward search structures
	TBitArray<>& VisitedForward = LocalAllocations->Visited;
	TArray<double>& GScoreForward = LocalAllocations->GScore;
	const TSharedPtr<PCGEx::FHashLookup> TravelStackForward = LocalAllocations->TravelStack;
	const TSharedPtr<PCGEx::FScoredQueue> QueueForward = LocalAllocations->ScoredQueue;

	// Backward search structures
	TBitArray<>& VisitedBackward = LocalAllocations->VisitedBackward;
	TArray<double>& GScoreBackward = LocalAllocations->GScoreBackward;
	const TSharedPtr<PCGEx::FHashLookup> TravelStackBackward = LocalAllocations->TravelStackBackward;
	const TSharedPtr<PCGEx::FScoredQueue> QueueBackward = LocalAllocations->ScoredQueueBackward;

	// Initialize forward search from seed
	QueueForward->Enqueue(SeedNode.Index, 0);
	GScoreForward[SeedNode.Index] = 0;

	// Initialize backward search from goal
	QueueBackward->Enqueue(GoalNode.Index, 0);
	GScoreBackward[GoalNode.Index] = 0;

	const PCGExHeuristics::FLocalFeedbackHandler* Feedback = LocalFeedback.Get();

	int32 MeetingNode = -1;
	double BestPathCost = MAX_dbl;

	// Alternate between forward and backward searches
	while (!QueueForward->IsEmpty() || !QueueBackward->IsEmpty())
	{
		// Forward step
		if (!QueueForward->IsEmpty())
		{
			int32 CurrentNodeIndex;
			double CurrentScore;
			QueueForward->Dequeue(CurrentNodeIndex, CurrentScore);

			// Check if we've found a better path
			if (CurrentScore >= BestPathCost) { continue; }

			// Check if backward search has reached this node
			if (VisitedBackward[CurrentNodeIndex])
			{
				const double PathCost = GScoreForward[CurrentNodeIndex] + GScoreBackward[CurrentNodeIndex];
				if (PathCost < BestPathCost)
				{
					BestPathCost = PathCost;
					MeetingNode = CurrentNodeIndex;
				}
			}

			if (!VisitedForward[CurrentNodeIndex])
			{
				VisitedForward[CurrentNodeIndex] = true;
				const PCGExClusters::FNode& Current = NodesRef[CurrentNodeIndex];
				const double CurrentGScore = GScoreForward[CurrentNodeIndex];

				for (const PCGExGraphs::FLink Lk : Current.Links)
				{
					const uint32 NeighborIndex = Lk.Node;
					const uint32 EdgeIndex = Lk.Edge;

					if (VisitedForward[NeighborIndex]) { continue; }

					const PCGExClusters::FNode& AdjacentNode = NodesRef[NeighborIndex];
					const PCGExGraphs::FEdge& Edge = EdgesRef[EdgeIndex];

					const double EScore = Heuristics->GetEdgeScore(Current, AdjacentNode, Edge, SeedNode, GoalNode, Feedback, TravelStackForward);
					const double TentativeGScore = CurrentGScore + EScore;

					const double PreviousGScore = GScoreForward[NeighborIndex];
					if (PreviousGScore != -1 && TentativeGScore >= PreviousGScore) { continue; }

					TravelStackForward->Set(NeighborIndex, PCGEx::NH64(CurrentNodeIndex, EdgeIndex));
					GScoreForward[NeighborIndex] = TentativeGScore;

					QueueForward->Enqueue(NeighborIndex, TentativeGScore);
				}
			}
		}

		// Backward step
		if (!QueueBackward->IsEmpty())
		{
			int32 CurrentNodeIndex;
			double CurrentScore;
			QueueBackward->Dequeue(CurrentNodeIndex, CurrentScore);

			if (CurrentScore >= BestPathCost) { continue; }

			// Check if forward search has reached this node
			if (VisitedForward[CurrentNodeIndex])
			{
				const double PathCost = GScoreForward[CurrentNodeIndex] + GScoreBackward[CurrentNodeIndex];
				if (PathCost < BestPathCost)
				{
					BestPathCost = PathCost;
					MeetingNode = CurrentNodeIndex;
				}
			}

			if (!VisitedBackward[CurrentNodeIndex])
			{
				VisitedBackward[CurrentNodeIndex] = true;
				const PCGExClusters::FNode& Current = NodesRef[CurrentNodeIndex];
				const double CurrentGScore = GScoreBackward[CurrentNodeIndex];

				for (const PCGExGraphs::FLink Lk : Current.Links)
				{
					const uint32 NeighborIndex = Lk.Node;
					const uint32 EdgeIndex = Lk.Edge;

					if (VisitedBackward[NeighborIndex]) { continue; }

					const PCGExClusters::FNode& AdjacentNode = NodesRef[NeighborIndex];
					const PCGExGraphs::FEdge& Edge = EdgesRef[EdgeIndex];

					// Note: For backward search, we reverse the direction conceptually
					const double EScore = Heuristics->GetEdgeScore(Current, AdjacentNode, Edge, GoalNode, SeedNode, Feedback, TravelStackBackward);
					const double TentativeGScore = CurrentGScore + EScore;

					const double PreviousGScore = GScoreBackward[NeighborIndex];
					if (PreviousGScore != -1 && TentativeGScore >= PreviousGScore) { continue; }

					TravelStackBackward->Set(NeighborIndex, PCGEx::NH64(CurrentNodeIndex, EdgeIndex));
					GScoreBackward[NeighborIndex] = TentativeGScore;

					QueueBackward->Enqueue(NeighborIndex, TentativeGScore);
				}
			}
		}

		// Early termination check
		if (MeetingNode != -1 && QueueForward->IsEmpty() && QueueBackward->IsEmpty()) { break; }
	}

	if (MeetingNode == -1) { return false; }

	// Reconstruct path
	ReconstructPath(InQuery, MeetingNode, TravelStackForward, TravelStackBackward, SeedNode.Index, GoalNode.Index);

	return true;
}

void FPCGExSearchOperationBidirectional::ReconstructPath(
	const TSharedPtr<PCGExPathfinding::FPathQuery>& InQuery,
	int32 MeetingNode,
	const TSharedPtr<PCGEx::FHashLookup>& ForwardStack,
	const TSharedPtr<PCGEx::FHashLookup>& BackwardStack,
	int32 SeedIndex,
	int32 GoalIndex) const
{
	// Build path from meeting point back to seed (will be reversed later)
	TArray<int32> ForwardPath;
	TArray<int32> ForwardEdges;

	int32 CurrentNode = MeetingNode;
	while (CurrentNode != SeedIndex)
	{
		ForwardPath.Add(CurrentNode);
		int32 PrevNode, EdgeIndex;
		PCGEx::NH64(ForwardStack->Get(CurrentNode), PrevNode, EdgeIndex);
		if (PrevNode == -1) { break; }
		ForwardEdges.Add(EdgeIndex);
		CurrentNode = PrevNode;
	}
	ForwardPath.Add(SeedIndex);

	// Reverse to get seed-to-meeting order
	Algo::Reverse(ForwardPath);
	Algo::Reverse(ForwardEdges);

	// Build path from meeting point to goal
	TArray<int32> BackwardPath;
	TArray<int32> BackwardEdges;

	CurrentNode = MeetingNode;
	while (CurrentNode != GoalIndex)
	{
		int32 NextNode, EdgeIndex;
		PCGEx::NH64(BackwardStack->Get(CurrentNode), NextNode, EdgeIndex);
		if (NextNode == -1) { break; }
		BackwardEdges.Add(EdgeIndex);
		BackwardPath.Add(NextNode);
		CurrentNode = NextNode;
	}

	// Combine paths (skip duplicate meeting node)
	// Add from seed to meeting point
	for (int i = 0; i < ForwardPath.Num(); i++)
	{
		InQuery->AddPathNode(ForwardPath[i], i < ForwardEdges.Num() ? ForwardEdges[i] : -1);
	}

	// Add from after meeting point to goal
	for (int i = 0; i < BackwardPath.Num(); i++)
	{
		InQuery->AddPathNode(BackwardPath[i], i < BackwardEdges.Num() ? BackwardEdges[i] : -1);
	}
}

TSharedPtr<PCGExPathfinding::FSearchAllocations> FPCGExSearchOperationBidirectional::NewAllocations() const
{
	TSharedPtr<PCGExPathfinding::FBidirectionalSearchAllocations> Allocations = MakeShared<PCGExPathfinding::FBidirectionalSearchAllocations>();
	Allocations->Init(Cluster);
	return Allocations;
}
