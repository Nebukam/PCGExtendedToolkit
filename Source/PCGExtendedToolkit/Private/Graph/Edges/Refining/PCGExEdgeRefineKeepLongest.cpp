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
		const double Dist = FVector::DistSquared(Node.Position, (Cluster->Nodes->GetData() + PCGEx::H64A(AdjacencyHash))->Position);
		if (Dist > LongestDist)
		{
			LongestDist = Dist;
			BestIndex = PCGEx::H64B(AdjacencyHash);
		}
	}

	if (BestIndex == -1) { return; }

	{
		//FWriteScopeLock WriteScopeLock(EdgeLock);
		(Cluster->Edges->GetData() + BestIndex)->bValid = true;
	}
}
