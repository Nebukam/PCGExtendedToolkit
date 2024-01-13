// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDirection.h"

double UPCGExHeuristicDirection::ComputeScore(
	const PCGExCluster::FScoredVertex* From,
	const PCGExCluster::FVertex& To,
	const PCGExCluster::FVertex& Seed,
	const PCGExCluster::FVertex& Goal, const PCGExGraph::FIndexedEdge& Edge) const
{
	return From->Score + FVector::DotProduct((From->Vertex->Position - To.Position).GetSafeNormal(), (From->Vertex->Position - Goal.Position).GetSafeNormal()) * 100;
}

bool UPCGExHeuristicDirection::IsBetterScore(const double NewScore, const double OtherScore) const
{
	return NewScore >= OtherScore;
}
