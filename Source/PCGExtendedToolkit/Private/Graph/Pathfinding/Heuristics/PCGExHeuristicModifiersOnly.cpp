// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicModifiersOnly.h"

bool UPCGExHeuristicModifiersOnly::IsBetterScore(const double NewScore, const double OtherScore) const
{	
	bool Result = Super::IsBetterScore(NewScore, OtherScore);
	return BaseInterpretation == EPCGExHeuristicScoreMode::HigherIsBetter ? !Result : Result;
}
