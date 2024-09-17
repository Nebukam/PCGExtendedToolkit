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
		const PCGExCluster::FNode* NoNode = new PCGExCluster::FNode();
		const int32 NumNodes = Cluster->Nodes->Num();

		TBitArray<> Visited;
		Visited.Init(false, NumNodes);

		PCGExSearch::TScoredQueue* ScoredQueue = new PCGExSearch::TScoredQueue(NumNodes, 0, 0);

		TArray<uint64> TravelStack;
		TravelStack.SetNum(NumNodes);

		for (int i = 0; i < NumNodes; ++i)
		{
			ScoredQueue->Scores[i] = TNumericLimits<double>::Max();
			TravelStack[i] = 0;
		}

		ScoredQueue->Scores[0] = 0;

		int32 CurrentNodeIndex;
		double CurrentNodeScore;
		while (ScoredQueue->Dequeue(CurrentNodeIndex, CurrentNodeScore))
		{
			const PCGExCluster::FNode& Current = *(Cluster->Nodes->GetData() + CurrentNodeIndex);
			Visited[CurrentNodeIndex] = true;

			for (const uint64 AdjacencyHash : Current.Adjacency)
			{
				uint32 NeighborIndex;
				uint32 EdgeIndex;
				PCGEx::H64(AdjacencyHash, NeighborIndex, EdgeIndex);

				if (Visited[NeighborIndex]) { continue; } // Exit early

				const PCGExCluster::FNode& AdjacentNode = *(Cluster->Nodes->GetData() + NeighborIndex);
				PCGExGraph::FIndexedEdge& Edge = *(Cluster->Edges->GetData() + EdgeIndex);

				const double Score = Heuristics->GetEdgeScore(Current, AdjacentNode, Edge, *NoNode, *NoNode, nullptr, &TravelStack);

				if (Score >= ScoredQueue->Scores[NeighborIndex]) { continue; }

				ScoredQueue->Scores[NeighborIndex] = Score;
				TravelStack[NeighborIndex] = PCGEx::H64(CurrentNodeIndex, EdgeIndex);

				ScoredQueue->Enqueue(NeighborIndex, Score);
			}
		}

		for (int32 i = 0; i < NumNodes; ++i)
		{
			uint32 NeighborIndex;
			uint32 EdgeIndex;

			PCGEx::H64(TravelStack[i], NeighborIndex, EdgeIndex);

			if (NeighborIndex == i) { continue; }

			(Cluster->Edges->GetData() + EdgeIndex)->bValid = true;
		}

		Visited.Empty();
		TravelStack.Empty();
		PCGEX_DELETE(ScoredQueue)
		PCGEX_DELETE(NoNode)
	}
};
