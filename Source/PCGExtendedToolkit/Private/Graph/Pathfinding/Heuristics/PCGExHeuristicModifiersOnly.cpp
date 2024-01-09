// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicModifiersOnly.h"

double UPCGExHeuristicModifiersOnly::ComputeScore(
	const PCGExCluster::FScoredVertex* From,
	const PCGExCluster::FVertex& To,
	const PCGExCluster::FVertex& Seed,
	const PCGExCluster::FVertex& Goal, const PCGExCluster::FIndexedEdge& Edge) const
{
	return 0;
}

bool UPCGExHeuristicModifiersOnly::IsBetterScore(const double NewScore, const double OtherScore) const
{
	return BaseInterpretation == EPCGExHeuristicScoreMode::HigherIsBetter ? NewScore >= OtherScore : NewScore <= OtherScore;
}
