// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDistance.h"

double UPCGExHeuristicDistance::ComputeFScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return FVector::DistSquared(Seed.Position, From.Position) + FVector::DistSquared(From.Position, Goal.Position);
}
