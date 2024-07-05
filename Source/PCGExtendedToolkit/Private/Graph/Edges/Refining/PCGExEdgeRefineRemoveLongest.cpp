// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Refining/PCGExEdgeRefineRemoveLongest.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"

bool UPCGExEdgeRemoveLongest::RequiresIndividualNodeProcessing()
{
	return true;
}

void UPCGExEdgeRemoveLongest::ProcessNode(PCGExCluster::FNode& Node, PCGExCluster::FCluster* InCluster, FRWLock& EdgeLock, PCGExHeuristics::THeuristicsHandler* InHeuristics)
{
	int32 BestIndex = -1;
	double LongestDist = TNumericLimits<double>::Min();

	for (const uint64 AdjacencyHash : Node.Adjacency)
	{
		const double Dist = FVector::DistSquared(Node.Position, (InCluster->Nodes->GetData() + PCGEx::H64A(AdjacencyHash))->Position);
		if (Dist > LongestDist)
		{
			LongestDist = Dist;
			BestIndex = PCGEx::H64B(AdjacencyHash);
		}
	}

	if (BestIndex == -1) { return; }

	{
		//FWriteScopeLock WriteScopeLock(EdgeLock);
		(InCluster->Edges->GetData() + BestIndex)->bValid = false;
	}
}
