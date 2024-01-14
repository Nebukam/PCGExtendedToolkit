// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDirection.h"

double UPCGExHeuristicDirection::ComputeScore(
	const PCGExCluster::FScoredNode* From,
	const PCGExCluster::FNode& To,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal, const PCGExGraph::FIndexedEdge& Edge) const
{
	return From->Score + FVector::DotProduct((From->Node->Position - To.Position).GetSafeNormal(), (From->Node->Position - Goal.Position).GetSafeNormal()) * 100;
}

bool UPCGExHeuristicDirection::IsBetterScore(const double NewScore, const double OtherScore) const
{
	return NewScore >= OtherScore;
}
