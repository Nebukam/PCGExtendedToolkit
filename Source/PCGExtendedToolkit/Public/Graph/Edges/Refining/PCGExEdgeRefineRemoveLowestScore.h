// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"
#include "PCGExEdgeRefineRemoveLowestScore.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Remove Lowest Score"))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeRemoveLowestScore : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual bool RequiresHeuristics() override { return true; }
	virtual bool RequiresIndividualNodeProcessing() override { return true; }

	virtual void ProcessNode(PCGExCluster::FNode& Node) override
	{
		int32 BestIndex = -1;
		double LowestScore = TNumericLimits<double>::Max();

		for (const uint64 AdjacencyHash : Node.Adjacency)
		{
			uint32 OtherNodeIndex;
			uint32 EdgeIndex;
			PCGEx::H64(AdjacencyHash, OtherNodeIndex, EdgeIndex);

			const double Score = Heuristics->GetEdgeScore(Node, *(Cluster->Nodes->GetData() + OtherNodeIndex), *(Cluster->Edges->GetData() + EdgeIndex), Node, *(Cluster->Nodes->GetData() + OtherNodeIndex));
			if (Score < LowestScore)
			{
				LowestScore = Score;
				BestIndex = EdgeIndex;
			}
		}

		if (BestIndex == -1) { return; }
		//if (!*(EdgesFilters->GetData() + BestIndex)) { return; }

		FPlatformAtomics::InterlockedExchange(&(Cluster->Edges->GetData() + BestIndex)->bValid, 0);
	}
};
