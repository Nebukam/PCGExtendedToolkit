// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"
#include "PCGExEdgeRefineRemoveLowestScore.generated.h"

/**
 * 
 */
class FPCGExEdgeRemoveLowestScore : public FPCGExEdgeRefineOperation
{
public:
	virtual void ProcessNode(PCGExCluster::FNode& Node) override
	{
		int32 BestIndex = -1;
		double LowestScore = MAX_dbl;

		const PCGExCluster::FNode& RoamingSeedNode = *Heuristics->GetRoamingSeed();
		const PCGExCluster::FNode& RoamingGoalNode = *Heuristics->GetRoamingGoal();

		for (const PCGExGraph::FLink Lk : Node.Links)
		{
			const double Score = Heuristics->GetEdgeScore(Node, *Cluster->GetNode(Lk), *Cluster->GetEdge(Lk), RoamingSeedNode, RoamingGoalNode);
			if (Score < LowestScore)
			{
				LowestScore = Score;
				BestIndex = Lk.Edge;
			}
		}

		if (BestIndex == -1) { return; }
		//if (!*(EdgesFilters->GetData() + BestIndex)) { return; }

		FPlatformAtomics::InterlockedExchange(&Cluster->GetEdge(BestIndex)->bValid, 0);
	}
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Remove Lowest Score", PCGExNodeLibraryDoc="clusters/refine-cluster/edge-score"))
class UPCGExEdgeRemoveLowestScore : public UPCGExEdgeRefineInstancedFactory
{
	GENERATED_BODY()

public:
	virtual bool WantsHeuristics() const override { return true; }
	virtual bool WantsIndividualNodeProcessing() const override { return true; }

	PCGEX_CREATE_REFINE_OPERATION(EdgeRemoveLowestScore, {})
};
