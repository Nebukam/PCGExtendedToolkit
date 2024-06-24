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
	double LongestDist = TNumericLimits<double>::Max();

	TArray<PCGExCluster::FNode>& NodesRef = (*InCluster->Nodes);

	for (const uint64 AdjacencyHash : Node.Adjacency)
	{
		uint32 OtherNodeIndex;
		uint32 EdgeIndex;
		PCGEx::H64(AdjacencyHash, OtherNodeIndex, EdgeIndex);

		const double Dist = FVector::DistSquared(Node.Position, NodesRef[OtherNodeIndex].Position);
		if (LongestDist < Dist)
		{
			LongestDist = Dist;
			BestIndex = EdgeIndex;
		}
	}

	if (BestIndex == -1) { return; }

	{
		FWriteScopeLock WriteScopeLock(EdgeLock);
		(*InCluster->Edges)[BestIndex].bValid = false;
	}
}
