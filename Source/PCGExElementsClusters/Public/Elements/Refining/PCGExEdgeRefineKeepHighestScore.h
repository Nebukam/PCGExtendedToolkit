// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "Clusters/PCGExCluster.h"
#include "PCGExEdgeRefineKeepHighestScore.generated.h"

/**
 * 
 */
class FPCGExEdgeKeepHighestScore : public FPCGExEdgeRefineOperation
{
public:
	virtual void ProcessNode(PCGExClusters::FNode& Node) override
	{
		int32 BestIndex = -1;
		double HighestScore = MIN_dbl_neg;

		const PCGExClusters::FNode& RoamingSeedNode = *Heuristics->GetRoamingSeed();
		const PCGExClusters::FNode& RoamingGoalNode = *Heuristics->GetRoamingGoal();

		for (const PCGExGraphs::FLink Lk : Node.Links)
		{
			const double Score = Heuristics->GetEdgeScore(Node, *Cluster->GetNode(Lk), *Cluster->GetEdge(Lk), RoamingSeedNode, RoamingGoalNode);
			if (Score > HighestScore)
			{
				HighestScore = Score;
				BestIndex = Lk.Edge;
			}
		}

		if (BestIndex == -1) { return; }
		//if (!*(EdgesFilters->GetData() + BestIndex)) { return; }

		FPlatformAtomics::InterlockedExchange(&Cluster->GetEdge(BestIndex)->bValid, 1);
	}
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Keep Highest Score", PCGExNodeLibraryDoc="clusters/refine-cluster/edge-score"))
class UPCGExEdgeKeepHighestScore : public UPCGExEdgeRefineInstancedFactory
{
	GENERATED_BODY()

public:
	virtual bool GetDefaultEdgeValidity() const override { return false; }
	virtual bool WantsHeuristics() const override { return true; }
	virtual bool WantsIndividualNodeProcessing() const override { return true; }

	PCGEX_CREATE_REFINE_OPERATION(EdgeKeepHighestScore, {})
};
