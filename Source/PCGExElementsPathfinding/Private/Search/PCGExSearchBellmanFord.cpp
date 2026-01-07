// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Search/PCGExSearchBellmanFord.h"

#include "PCGExHeuristicsHandler.h"
#include "Clusters/PCGExCluster.h"
#include "Containers/PCGExHashLookup.h"
#include "Core/PCGExPathfinding.h"
#include "Core/PCGExPathQuery.h"
#include "Core/PCGExSearchAllocations.h"

bool FPCGExSearchOperationBellmanFord::ResolveQuery(
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

	const int32 NumNodes = NodesRef.Num();

	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSearchOperationBellmanFord::FindPath);

	TArray<double>& Distance = LocalAllocations->GScore;
	const TSharedPtr<PCGEx::FHashLookup> TravelStack = LocalAllocations->TravelStack;

	const PCGExHeuristics::FLocalFeedbackHandler* Feedback = LocalFeedback.Get();

	// Initialize distances
	Distance[SeedNode.Index] = 0;

	// Relax all edges |V| - 1 times
	for (int32 Iteration = 0; Iteration < NumNodes - 1; Iteration++)
	{
		bool bAnyRelaxation = false;

		// For each node, check all outgoing edges
		for (int32 NodeIndex = 0; NodeIndex < NumNodes; NodeIndex++)
		{
			const double CurrentDist = Distance[NodeIndex];
			if (CurrentDist == MAX_dbl) { continue; } // Not yet reachable

			const PCGExClusters::FNode& CurrentNode = NodesRef[NodeIndex];

			for (const PCGExGraphs::FLink Lk : CurrentNode.Links)
			{
				const uint32 NeighborIndex = Lk.Node;
				const uint32 EdgeIndex = Lk.Edge;

				const PCGExClusters::FNode& AdjacentNode = NodesRef[NeighborIndex];
				const PCGExGraphs::FEdge& Edge = EdgesRef[EdgeIndex];

				const double EdgeWeight = Heuristics->GetEdgeScore(CurrentNode, AdjacentNode, Edge, SeedNode, GoalNode, Feedback, TravelStack);
				const double NewDist = CurrentDist + EdgeWeight;

				if (NewDist < Distance[NeighborIndex])
				{
					Distance[NeighborIndex] = NewDist;
					TravelStack->Set(NeighborIndex, PCGEx::NH64(NodeIndex, EdgeIndex));
					bAnyRelaxation = true;
				}
			}
		}

		// Early termination if no relaxation occurred
		if (!bAnyRelaxation) { break; }

		// Early exit if goal is reachable and we want to exit early
		if (bEarlyExit && Distance[GoalNode.Index] != MAX_dbl && !bAnyRelaxation) { break; }
	}

	// Check for negative weight cycles if requested
	if (bDetectNegativeCycles)
	{
		for (int32 NodeIndex = 0; NodeIndex < NumNodes; NodeIndex++)
		{
			const double CurrentDist = Distance[NodeIndex];
			if (CurrentDist == MAX_dbl) { continue; }

			const PCGExClusters::FNode& CurrentNode = NodesRef[NodeIndex];

			for (const PCGExGraphs::FLink Lk : CurrentNode.Links)
			{
				const uint32 NeighborIndex = Lk.Node;
				const uint32 EdgeIndex = Lk.Edge;

				const PCGExClusters::FNode& AdjacentNode = NodesRef[NeighborIndex];
				const PCGExGraphs::FEdge& Edge = EdgesRef[EdgeIndex];

				const double EdgeWeight = Heuristics->GetEdgeScore(CurrentNode, AdjacentNode, Edge, SeedNode, GoalNode, Feedback, TravelStack);

				// If we can still relax, there's a negative cycle
				if (CurrentDist + EdgeWeight < Distance[NeighborIndex])
				{
					// Negative cycle detected - path finding fails
					return false;
				}
			}
		}
	}

	// Check if goal is reachable
	if (Distance[GoalNode.Index] == MAX_dbl) { return false; }

	// Reconstruct path
	int32 PathNodeIndex = PCGEx::NH64A(TravelStack->Get(GoalNode.Index));
	int32 PathEdgeIndex = -1;

	if (PathNodeIndex != -1)
	{
		InQuery->AddPathNode(GoalNode.Index);

		while (PathNodeIndex != -1)
		{
			const int32 CurrentIndex = PathNodeIndex;
			PCGEx::NH64(TravelStack->Get(CurrentIndex), PathNodeIndex, PathEdgeIndex);
			InQuery->AddPathNode(CurrentIndex, PathEdgeIndex);
		}

		return true;
	}

	return false;
}

TSharedPtr<PCGExPathfinding::FSearchAllocations> FPCGExSearchOperationBellmanFord::NewAllocations() const
{
	TSharedPtr<PCGExPathfinding::FSearchAllocations> NewAllocations = FPCGExSearchOperation::NewAllocations();
	NewAllocations->GScore.Init(MAX_dbl, Cluster->Nodes->Num()); // Use MAX_dbl as infinity
	return NewAllocations;
}

void UPCGExSearchBellmanFord::CopySettingsFrom(const UPCGExInstancedFactory* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExSearchBellmanFord* TypedOther = Cast<UPCGExSearchBellmanFord>(Other))
	{
		bDetectNegativeCycles = TypedOther->bDetectNegativeCycles;
	}
}