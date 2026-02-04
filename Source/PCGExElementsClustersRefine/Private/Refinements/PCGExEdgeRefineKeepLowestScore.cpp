// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Refinements/PCGExEdgeRefineKeepLowestScore.h"

#pragma region FPCGExEdgeKeepLowestScore

void FPCGExEdgeKeepLowestScore::ProcessNode(PCGExClusters::FNode& Node)
{
	int32 BestIndex = -1;
	double LowestScore = MAX_dbl;

	const PCGExClusters::FNode& RoamingSeedNode = *Heuristics->GetRoamingSeed();
	const PCGExClusters::FNode& RoamingGoalNode = *Heuristics->GetRoamingGoal();

	for (const PCGExGraphs::FLink Lk : Node.Links)
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

	FPlatformAtomics::InterlockedExchange(&Cluster->GetEdge(BestIndex)->bValid, 1);
}

#pragma endregion
