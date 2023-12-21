// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDistance.h"

double UPCGExHeuristicDistance::ComputeScore(
	const PCGExMesh::FScoredVertex* From,
	const PCGExMesh::FVertex& To,
	const PCGExMesh::FVertex& Seed,
	const PCGExMesh::FVertex& Goal, const PCGExMesh::FIndexedEdge& Edge) const
{
	return FVector::Distance(Goal.Position, To.Position);
}

bool UPCGExHeuristicDistance::IsBetterScore(const double NewScore, const double OtherScore) const
{
	return NewScore <= OtherScore;
}
