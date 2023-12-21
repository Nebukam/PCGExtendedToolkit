// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicModifiersOnly.h"

double UPCGExHeuristicModifiersOnly::ComputeScore(
	const PCGExMesh::FScoredVertex* From,
	const PCGExMesh::FVertex& To,
	const PCGExMesh::FVertex& Seed,
	const PCGExMesh::FVertex& Goal, const PCGExMesh::FIndexedEdge& Edge) const
{
	return 0;
}

bool UPCGExHeuristicModifiersOnly::IsBetterScore(const double NewScore, const double OtherScore) const
{
	return BaseInterpretation == EPCGExHeuristicScoreMode::HigherIsBetter ? NewScore >= OtherScore : NewScore <= OtherScore;
}
