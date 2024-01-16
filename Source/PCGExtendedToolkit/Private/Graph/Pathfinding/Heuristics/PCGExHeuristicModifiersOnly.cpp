// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicModifiersOnly.h"

double UPCGExHeuristicModifiersOnly::ComputeScore(
	const PCGExCluster::FScoredNode* From,
	const PCGExCluster::FNode& To,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal, const PCGExGraph::FIndexedEdge& Edge) const
{
	return 0;
}

bool UPCGExHeuristicModifiersOnly::IsBetterScore(const double NewScore, const double OtherScore) const
{	
	bool Result = Super::IsBetterScore(NewScore, OtherScore);
	return BaseInterpretation == EPCGExHeuristicScoreMode::HigherIsBetter ? !Result : Result;
}
