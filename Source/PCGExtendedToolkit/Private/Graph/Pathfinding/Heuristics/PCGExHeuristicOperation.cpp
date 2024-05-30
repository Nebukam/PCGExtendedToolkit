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
	return 0;
}

double UPCGExHeuristicOperation::GetEdgeScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& To,
	const PCGExGraph::FIndexedEdge& Edge,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return 0;
}

void UPCGExHeuristicOperation::Cleanup()
{
	Cluster = nullptr;
	Super::Cleanup();
}

double UPCGExHeuristicOperation::SampleCurve(const double InTime) const
{
	return FMath::Max(0, ScoreCurveObj->GetFloatValue(bInvert ? 1 - InTime : InTime));
}
