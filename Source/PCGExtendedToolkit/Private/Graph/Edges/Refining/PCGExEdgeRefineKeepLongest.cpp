// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Refining/PCGExEdgeRefineKeepLongest.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"

void UPCGExEdgeKeepLongest::ProcessNode(PCGExCluster::FNode& Node)
{
	int32 BestIndex = -1;
	double LongestDist = TNumericLimits<double>::Min();

	for (const uint64 AdjacencyHash : Node.Adjacency)
	{
		uint32 OtherNodeIndex;
		uint32 EdgeIndex;
		PCGEx::H64(AdjacencyHash, OtherNodeIndex, EdgeIndex);

		if (!*(EdgesFilters->GetData() + EdgeIndex)) { continue; }

		const double Dist = Cluster->GetDistSquared(Node.NodeIndex, OtherNodeIndex);
		if (Dist > LongestDist)
		{
			LongestDist = Dist;
			BestIndex = PCGEx::H64B(AdjacencyHash);
		}
	}

	if (BestIndex == -1) { return; }
	FPlatformAtomics::InterlockedExchange(&(Cluster->Edges->GetData() + BestIndex)->bValid, 1);
}
