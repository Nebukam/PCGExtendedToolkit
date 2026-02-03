// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Refinements/PCGExEdgeRefineRemoveShortest.h"

#pragma region FPCGExEdgeRemoveShortest

void FPCGExEdgeRemoveShortest::ProcessNode(PCGExClusters::FNode& Node)
{
	int32 BestIndex = -1;
	double ShortestDist = MAX_dbl;

	for (const PCGExGraphs::FLink Lk : Node.Links)
	{
		const double Dist = Cluster->GetDistSquared(Node.Index, Lk.Node);
		if (Dist < ShortestDist)
		{
			ShortestDist = Dist;
			BestIndex = Lk.Edge;
		}
	}

	if (BestIndex == -1) { return; }
	//if (!*(EdgesFilters->GetData() + BestIndex)) { return; }

	FPlatformAtomics::InterlockedExchange(&Cluster->GetEdge(BestIndex)->bValid, 0);
}

#pragma endregion
