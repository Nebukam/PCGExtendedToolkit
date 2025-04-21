// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"
#include "PCGExEdgeRefineRemoveHighestScore.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Remove Highest Score"))
class UPCGExEdgeRemoveHighestScore : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual bool WantsHeuristics() override { return true; }
	virtual bool WantsIndividualNodeProcessing() override { return true; }

	virtual void ProcessNode(PCGExCluster::FNode& Node) override
	{
		int32 BestIndex = -1;
		double HighestScore = MIN_dbl_neg;

		for (const PCGExGraph::FLink Lk : Node.Links)
		{
			const double Score = Heuristics->GetEdgeScore(Node, *Cluster->GetNode(Lk), *Cluster->GetEdge(Lk), *RoamingSeedNode, *RoamingGoalNode);
			if (Score > HighestScore)
			{
				HighestScore = Score;
				BestIndex = Lk.Edge;
			}
		}

		if (BestIndex == -1) { return; }
		//if (!*(EdgesFilters->GetData() + BestIndex)) { return; }

		FPlatformAtomics::InterlockedExchange(&Cluster->GetEdge(BestIndex)->bValid, 0);
	}
};
