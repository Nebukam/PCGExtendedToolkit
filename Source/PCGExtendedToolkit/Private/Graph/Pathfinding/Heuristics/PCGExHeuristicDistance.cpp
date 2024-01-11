// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDistance.h"

double UPCGExHeuristicDistance::ComputeScore(
	const PCGExCluster::FScoredVertex* From,
	const PCGExCluster::FVertex& To,
	const PCGExCluster::FVertex& Seed,
	const PCGExCluster::FVertex& Goal, const PCGExGraph::FIndexedEdge& Edge) const
{
	return FVector::Distance(Goal.Position, To.Position);
}

bool UPCGExHeuristicDistance::IsBetterScore(const double NewScore, const double OtherScore) const
{
	return NewScore <= OtherScore;
}
