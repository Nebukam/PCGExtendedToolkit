// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicOperation.h"

void UPCGExHeuristicOperation::PrepareForData(const PCGExCluster::FCluster* InCluster)
{
}

double UPCGExHeuristicOperation::ComputeScore(
	const PCGExCluster::FScoredNode* From,
	const PCGExCluster::FNode& To,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal,
	const PCGExGraph::FIndexedEdge& Edge) const
{
	return From->Score + 1;
}

bool UPCGExHeuristicOperation::IsBetterScore(const double NewScore, const double OtherScore) const
{
	return NewScore <= OtherScore;
}

int32 UPCGExHeuristicOperation::GetQueueingIndex(const TArray<PCGExCluster::FScoredNode*>& InVertices, const double InScore) const
{
	for (int i = InVertices.Num() - 1; i >= 0; i--)
	{
		if (IsBetterScore(InScore, InVertices[i]->Score)) { return i + 1; }
	}
	return -1;
}
