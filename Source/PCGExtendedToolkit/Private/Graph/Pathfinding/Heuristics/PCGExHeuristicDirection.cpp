// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDirection.h"


double UPCGExHeuristicDirection::ComputeFScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	FVector Dir = (Seed.Position - Goal.Position).GetSafeNormal();
	return FVector::DotProduct(Dir, (From.Position - Goal.Position).GetSafeNormal()) * -1;
}

double UPCGExHeuristicDirection::ComputeDScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& To,
	const PCGExGraph::FIndexedEdge& Edge,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return (FVector::DotProduct((From.Position - To.Position).GetSafeNormal(), (From.Position - Goal.Position).GetSafeNormal()) * -1) * ReferenceWeight;
}
