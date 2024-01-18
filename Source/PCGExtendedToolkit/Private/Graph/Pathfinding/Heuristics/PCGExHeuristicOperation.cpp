// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicOperation.h"

void UPCGExHeuristicOperation::PrepareForData(PCGExCluster::FCluster* InCluster)
{
	Cluster = InCluster;
}

double UPCGExHeuristicOperation::GetGlobalScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return 0;
}

double UPCGExHeuristicOperation::GetEdgeScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& To,
	const PCGExGraph::FIndexedEdge& Edge,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return 1;
}

bool UPCGExHeuristicOperation::IsBetterScore(const double NewScore, const double OtherScore) const // Defaults to lower is better
{
	return NewScore < OtherScore;
}

int32 UPCGExHeuristicOperation::GetQueueingIndex(const TArray<PCGExCluster::FScoredNode*>& InList, const double InScore, const int32 A) const
{
	const int32 MaxIndex = InList.Num() - 1;
	for (int i = MaxIndex; i >= 0; i--) { if (IsBetterScore(InScore, InList[i]->Score)) { return i + 1; } }
	return FMath::Max(0, MaxIndex);
}

void UPCGExHeuristicOperation::ScoredInsert(TArray<PCGExCluster::FScoredNode*>& InList, PCGExCluster::FScoredNode* Node) const
{
	int32 TargetIndex = 0;
	const int32 MaxIndex = InList.Num() - 1;

	for (int i = MaxIndex; i >= 0; i--)
	{
		if (IsBetterScore(Node->Score, InList[i]->Score))
		{
			TargetIndex = i + 1;
			break;
		}
	}

	InList.Insert(Node, TargetIndex);
}

void UPCGExHeuristicOperation::Cleanup()
{
	Cluster = nullptr;
	Super::Cleanup();
}
