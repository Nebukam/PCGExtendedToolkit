// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicLocalDistance.h"

void UPCGExHeuristicLocalDistance::PrepareForData(PCGExCluster::FCluster* InCluster)
{
	InCluster->ComputeEdgeLengths(true);
	Super::PrepareForData(InCluster);
}

double UPCGExHeuristicLocalDistance::GetEdgeScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& To,
	const PCGExGraph::FIndexedEdge& Edge,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return FMath::Max(0, ScoreCurveObj->GetFloatValue(Cluster->EdgeLengths[Edge.EdgeIndex])) * ReferenceWeight;
}
