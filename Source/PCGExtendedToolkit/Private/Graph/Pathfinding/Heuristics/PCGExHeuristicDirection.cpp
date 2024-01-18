// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDirection.h"


double UPCGExHeuristicDirection::GetGlobalScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	const FVector Dir = (Seed.Position - Goal.Position).GetSafeNormal();
	const double Dot = FVector::DotProduct(Dir, (From.Position - Goal.Position).GetSafeNormal()) * -1;
	return PCGExMath::Remap(Dot, -1, 1, 0, 1) * ReferenceWeight;
}

double UPCGExHeuristicDirection::GetEdgeScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& To,
	const PCGExGraph::FIndexedEdge& Edge,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	const double Dot = (FVector::DotProduct((From.Position - To.Position).GetSafeNormal(), (From.Position - Goal.Position).GetSafeNormal()) * -1);
	return PCGExMath::Remap(Dot, -1, 1, 0, 1) * ReferenceWeight;
}
