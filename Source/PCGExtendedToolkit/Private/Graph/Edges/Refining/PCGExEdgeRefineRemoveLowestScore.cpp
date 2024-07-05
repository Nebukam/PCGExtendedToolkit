// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Refining/PCGExEdgeRefineRemoveLowestScore.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"

bool UPCGExEdgeRemoveLowestScore::RequiresHeuristics()
{
	return true;
}

bool UPCGExEdgeRemoveLowestScore::RequiresIndividualNodeProcessing()
{
	return true;
}

void UPCGExEdgeRemoveLowestScore::ProcessNode(PCGExCluster::FNode& Node, PCGExCluster::FCluster* InCluster, FRWLock& EdgeLock, PCGExHeuristics::THeuristicsHandler* InHeuristics)
{
	int32 BestIndex = -1;
	double LowestScore = TNumericLimits<double>::Max();

	for (const uint64 AdjacencyHash : Node.Adjacency)
	{
		uint32 OtherNodeIndex;
		uint32 EdgeIndex;
		PCGEx::H64(AdjacencyHash, OtherNodeIndex, EdgeIndex);

		const double Score = InHeuristics->GetEdgeScore(Node, *(InCluster->Nodes->GetData() + OtherNodeIndex), *(InCluster->Edges->GetData() + EdgeIndex), Node, *(InCluster->Nodes->GetData() + OtherNodeIndex));
		if (Score < LowestScore)
		{
			LowestScore = Score;
			BestIndex = EdgeIndex;
		}
	}

	if (BestIndex == -1) { return; }

	{
		FWriteScopeLock WriteScopeLock(EdgeLock);
		(InCluster->Edges->GetData() + BestIndex)->bValid = false;
	}
}
