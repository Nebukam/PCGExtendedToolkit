// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Refining/PCGExEdgeRefineRemoveShortest.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"

void UPCGExEdgeRemoveShortest::ProcessNode(PCGExCluster::FNode& Node)
{
	int32 BestIndex = -1;
	double ShortestDist = TNumericLimits<double>::Max();

	for (const uint64 AdjacencyHash : Node.Adjacency)
	{
		const double Dist = Cluster->GetDistSquared(Node.NodeIndex, PCGEx::H64A(AdjacencyHash));
		if (Dist < ShortestDist)
		{
			ShortestDist = Dist;
			BestIndex = PCGEx::H64B(AdjacencyHash);
		}
	}

	if (BestIndex == -1) { return; }

	FPlatformAtomics::InterlockedExchange(&(Cluster->Edges->GetData() + BestIndex)->bValid, 0);
}
