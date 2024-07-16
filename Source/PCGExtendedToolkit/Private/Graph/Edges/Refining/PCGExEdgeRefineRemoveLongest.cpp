// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Refining/PCGExEdgeRefineRemoveLongest.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"

void UPCGExEdgeRemoveLongest::ProcessNode(PCGExCluster::FNode& Node)
{
	int32 BestIndex = -1;
	double LongestDist = TNumericLimits<double>::Min();

	for (const uint64 AdjacencyHash : Node.Adjacency)
	{
		const double Dist = Cluster->GetDistSquared(Node.NodeIndex, PCGEx::H64A(AdjacencyHash));
		if (Dist > LongestDist)
		{
			LongestDist = Dist;
			BestIndex = PCGEx::H64B(AdjacencyHash);
		}
	}

	if (BestIndex == -1) { return; }

	{
		//FWriteScopeLock WriteScopeLock(EdgeLock);
		(Cluster->Edges->GetData() + BestIndex)->bValid = false;
	}
}
