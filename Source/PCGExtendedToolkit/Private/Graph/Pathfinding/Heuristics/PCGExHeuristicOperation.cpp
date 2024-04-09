// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicOperation.h"

void UPCGExHeuristicOperation::PrepareForData(PCGExCluster::FCluster* InCluster)
{
	Cluster = InCluster;

	if (!ScoreCurve || ScoreCurve.IsNull()) { ScoreCurveObj = TSoftObjectPtr<UCurveFloat>(PCGEx::WeightDistributionLinear).LoadSynchronous(); }
	else { ScoreCurveObj = ScoreCurve.LoadSynchronous(); }
}

double UPCGExHeuristicOperation::GetGlobalScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return 0;
}

double UPCGExHeuristicOperation::GetEdgeScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& To,
	const PCGExGraph::FIndexedEdge& Edge,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return 1;
}

void UPCGExHeuristicOperation::Cleanup()
{
	Cluster = nullptr;
	Super::Cleanup();
}
