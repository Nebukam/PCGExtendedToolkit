// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"
#include "Graph/Pathfinding/Search/PCGExScoredQueue.h"
#include "PCGExEdgeRefinePrimMST.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Refine : MST (Prim)"))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeRefinePrimMST : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual bool GetDefaultEdgeValidity() override { return false; }
	virtual bool RequiresHeuristics() override { return true; }

	virtual void Process() override
	{
		const int32 NumNodes = Cluster->Nodes->Num();

		TBitArray<> Visited;
		Visited.Init(false, NumNodes);

		const TUniquePtr<PCGExSearch::FScoredQueue> ScoredQueue = MakeUnique<PCGExSearch::FScoredQueue>(NumNodes, RoamingSeedNode->Index, 0);
		const TSharedPtr<PCGEx::FHashLookup> TravelStack = PCGEx::NewHashLookup<PCGEx::FArrayHashLookup>(PCGEx::NH64(-1, -1), NumNodes);

		int32 CurrentNodeIndex;
		double CurrentNodeScore;
		while (ScoredQueue->Dequeue(CurrentNodeIndex, CurrentNodeScore))
		{
			const PCGExCluster::FNode& Current = *Cluster->GetNode(CurrentNodeIndex);
			Visited[CurrentNodeIndex] = true;

			for (const PCGExGraph::FLink Lk : Current.Links)
			{
				const uint32 NeighborIndex = Lk.Node;
				const uint32 EdgeIndex = Lk.Edge;

				if (Visited[NeighborIndex]) { continue; } // Exit early

				const PCGExCluster::FNode& AdjacentNode = *Cluster->GetNode(NeighborIndex);
				PCGExGraph::FEdge& Edge = *Cluster->GetEdge(EdgeIndex);

				const double Score = Heuristics->GetEdgeScore(Current, AdjacentNode, Edge, *RoamingSeedNode, *RoamingGoalNode, nullptr, TravelStack);
				if (!ScoredQueue->Enqueue(NeighborIndex, Score)) { continue; }

				TravelStack->Set(NeighborIndex, PCGEx::H64(CurrentNodeIndex, EdgeIndex));
			}
		}

		for (int32 i = 0; i < NumNodes; i++)
		{
			uint32 Node;
			uint32 EdgeIndex;

			PCGEx::H64(TravelStack->Get(i), Node, EdgeIndex);
			if (Node == i) { continue; }

			Cluster->GetEdge(EdgeIndex)->bValid = true;
		}
	}
};
