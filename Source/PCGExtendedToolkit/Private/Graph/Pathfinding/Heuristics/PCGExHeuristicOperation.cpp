// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicOperation.h"

void UPCGExHeuristicOperation::PrepareForData(PCGExCluster::FCluster* InCluster)
{
	Cluster = InCluster;
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

bool UPCGExHeuristicOperation::IsBetterScore(const double NewScore, const double OtherScore, const int32 A, const int32 B) const
{
	return FMath::IsNearlyEqual(NewScore, OtherScore) ? A < B : NewScore < OtherScore; // Defaults to lower is better
}

int32 UPCGExHeuristicOperation::GetQueueingIndex(const TArray<PCGExCluster::FScoredNode*>& InList, const double InScore, const int32 A) const
{
	for (int i = InList.Num() - 1; i >= 0; i--)
	{
		if (IsBetterScore(InScore, InList[i]->Score, A, InList[i]->Node->PointIndex)) { return i + 1; }
	}
	return -1;
}

void UPCGExHeuristicOperation::Cleanup()
{
	Cluster = nullptr;
	Super::Cleanup();
}
