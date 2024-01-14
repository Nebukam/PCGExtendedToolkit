// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDistance.h"

double UPCGExHeuristicDistance::ComputeScore(
	const PCGExCluster::FScoredNode* From,
	const PCGExCluster::FNode& To,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal, const PCGExGraph::FIndexedEdge& Edge) const
{
	return FVector::Distance(Goal.Position, To.Position);
}

bool UPCGExHeuristicDistance::IsBetterScore(const double NewScore, const double OtherScore) const
{
	return NewScore <= OtherScore;
}
