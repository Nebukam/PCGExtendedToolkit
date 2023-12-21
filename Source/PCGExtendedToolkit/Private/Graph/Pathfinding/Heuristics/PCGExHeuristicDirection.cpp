// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDirection.h"

double UPCGExHeuristicDirection::ComputeScore(
	const PCGExMesh::FScoredVertex* From,
	const PCGExMesh::FVertex& To,
	const PCGExMesh::FVertex& Seed,
	const PCGExMesh::FVertex& Goal, const PCGExMesh::FIndexedEdge& Edge) const
{
	return From->Score + FVector::DotProduct((From->Vertex->Position - To.Position).GetSafeNormal(), (From->Vertex->Position - Goal.Position).GetSafeNormal()) * 100;
}

bool UPCGExHeuristicDirection::IsBetterScore(const double NewScore, const double OtherScore) const
{
	return NewScore >= OtherScore;
}
