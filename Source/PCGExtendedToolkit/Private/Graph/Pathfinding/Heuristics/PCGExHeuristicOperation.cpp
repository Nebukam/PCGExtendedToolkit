// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicOperation.h"

void UPCGExHeuristicOperation::PrepareForCluster(PCGExCluster::FCluster* InCluster)
{
	Cluster = InCluster;
	PCGEX_LOAD_SOFTOBJECT(UCurveFloat, ScoreCurve, ScoreCurveObj, PCGEx::WeightDistributionLinear)
}

double UPCGExHeuristicOperation::GetGlobalScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return ReferenceWeight;
}

double UPCGExHeuristicOperation::GetEdgeScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& To,
	const PCGExGraph::FIndexedEdge& Edge,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return ReferenceWeight;
}

void UPCGExHeuristicOperation::Cleanup()
{
	Cluster = nullptr;
	Super::Cleanup();
}
