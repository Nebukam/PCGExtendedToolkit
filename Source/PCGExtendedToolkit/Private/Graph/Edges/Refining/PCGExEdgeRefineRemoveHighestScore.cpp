// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Refining/PCGExEdgeRefineRemoveHighestScore.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"

bool UPCGExEdgeRemoveHighestScore::RequiresHeuristics()
{
	return true;
}

bool UPCGExEdgeRemoveHighestScore::RequiresIndividualNodeProcessing()
{
	return true;
}

void UPCGExEdgeRemoveHighestScore::ProcessNode(PCGExCluster::FNode& Node, PCGExCluster::FCluster* InCluster, FRWLock& EdgeLock, PCGExHeuristics::THeuristicsHandler* InHeuristics)
{
	int32 BestIndex = -1;
	double HighestScore = TNumericLimits<double>::Min();

	for (const uint64 AdjacencyHash : Node.Adjacency)
	{
		uint32 OtherNodeIndex;
		uint32 EdgeIndex;
		PCGEx::H64(AdjacencyHash, OtherNodeIndex, EdgeIndex);

		const double Score = InHeuristics->GetEdgeScore(Node, *(InCluster->Nodes->GetData() + OtherNodeIndex), *(InCluster->Edges->GetData() + EdgeIndex), Node, *(InCluster->Nodes->GetData() + OtherNodeIndex));
		if (Score > HighestScore)
		{
			HighestScore = Score;
			BestIndex = EdgeIndex;
		}
	}

	if (BestIndex == -1) { return; }

	{
		FWriteScopeLock WriteScopeLock(EdgeLock);
		(InCluster->Edges->GetData() + BestIndex)->bValid = false;
	}
}
