// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDirection.h"

double UPCGExHeuristicDirection::ComputeScore(
	const PCGExCluster::FScoredNode* From,
	const PCGExCluster::FNode& To,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal, const PCGExGraph::FIndexedEdge& Edge) const
{
	return FVector::DotProduct((From->Node->Position - To.Position).GetSafeNormal(), (From->Node->Position - Goal.Position).GetSafeNormal()) * ReferenceWeight;
}

bool UPCGExHeuristicDirection::IsBetterScore(const double NewScore, const double OtherScore, const int32 A, const int32 B) const
{
	return FMath::IsNearlyEqual(NewScore, OtherScore) ? A > B : NewScore > OtherScore;
}
