// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Refining/PCGExEdgeRefineRemoveShortest.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"

bool UPCGExEdgeRemoveShortest::RequiresIndividualNodeProcessing()
{
	return true;
}

void UPCGExEdgeRemoveShortest::ProcessNode(PCGExCluster::FNode& Node, PCGExCluster::FCluster* InCluster, FRWLock& EdgeLock, PCGExHeuristics::THeuristicsHandler* InHeuristics)
{
	int32 BestIndex = -1;
	double ShortestDist = TNumericLimits<double>::Min();

	TArray<PCGExCluster::FNode>& NodesRef = (*InCluster->Nodes);

	for (const uint64 AdjacencyHash : Node.Adjacency)
	{
		uint32 OtherNodeIndex;
		uint32 EdgeIndex;
		PCGEx::H64(AdjacencyHash, OtherNodeIndex, EdgeIndex);

		const double Dist = FVector::DistSquared(Node.Position, NodesRef[OtherNodeIndex].Position);
		if (ShortestDist > Dist)
		{
			ShortestDist = Dist;
			BestIndex = EdgeIndex;
		}
	}

	if (BestIndex == -1) { return; }

	{
		FWriteScopeLock WriteScopeLock(EdgeLock);
		(*InCluster->Edges)[BestIndex].bValid = false;
	}
}
