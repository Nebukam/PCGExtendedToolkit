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
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="MST (Prim)"))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeRefinePrimMST : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual bool GetDefaultEdgeValidity() override { return false; }
	virtual bool RequiresHeuristics() override { return true; }
	
	virtual void Process() override
	{
		const TUniquePtr<PCGExCluster::FNode> NoNodePtr = MakeUnique<PCGExCluster::FNode>();
		const PCGExCluster::FNode& NoNode = *NoNodePtr.Get();
		const int32 NumNodes = Cluster->Nodes->Num();

		TBitArray<> Visited;
		Visited.Init(false, NumNodes);

		const TUniquePtr<PCGExSearch::TScoredQueue> ScoredQueue = MakeUnique<PCGExSearch::TScoredQueue>(NumNodes, 0, 0);

		TArray<uint64> TravelStack;
		TravelStack.Init(0, NumNodes);

		ScoredQueue->Scores[0] = 0;

		int32 CurrentNodeIndex;
		double CurrentNodeScore;
		while (ScoredQueue->Dequeue(CurrentNodeIndex, CurrentNodeScore))
		{
			const PCGExCluster::FNode& Current = *Cluster->GetNode(CurrentNodeIndex);
			Visited[CurrentNodeIndex] = true;

			for (const uint64 AdjacencyHash : Current.Adjacency)
			{
				uint32 NeighborIndex;
				uint32 EdgeIndex;
				PCGEx::H64(AdjacencyHash, NeighborIndex, EdgeIndex);

				if (Visited[NeighborIndex]) { continue; } // Exit early

				const PCGExCluster::FNode& AdjacentNode = *Cluster->GetNode(NeighborIndex);
				PCGExGraph::FIndexedEdge& Edge = *Cluster->GetEdge(EdgeIndex);

				const double Score = Heuristics->GetEdgeScore(Current, AdjacentNode, Edge, NoNode, NoNode, nullptr, &TravelStack);
				if (!ScoredQueue->Enqueue(NeighborIndex, Score)) { continue; }

				TravelStack[NeighborIndex] = PCGEx::H64(CurrentNodeIndex, EdgeIndex);
			}
		}

		for (int32 i = 0; i < NumNodes; i++)
		{
			uint32 Node;
			uint32 EdgeIndex;

			PCGEx::H64(TravelStack[i], Node, EdgeIndex);
			if (Node == i) { continue; }

			Cluster->GetEdge(EdgeIndex)->bValid = true;
		}
	}
};
