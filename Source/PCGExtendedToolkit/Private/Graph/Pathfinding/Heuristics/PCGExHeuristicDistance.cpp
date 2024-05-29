// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDistance.h"

void UPCGExHeuristicDistance::PrepareForCluster(PCGExCluster::FCluster* InCluster)
{
	MaxDistSquared = FVector::DistSquared(InCluster->Bounds.Min, InCluster->Bounds.Max);
	Super::PrepareForCluster(InCluster);
}

double UPCGExHeuristicDistance::GetGlobalScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return (FVector::DistSquared(From.Position, Goal.Position) / MaxDistSquared) * ReferenceWeight;
}

double UPCGExHeuristicDistance::GetEdgeScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& To,
	const PCGExGraph::FIndexedEdge& Edge,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return FMath::Max(0, ScoreCurveObj->GetFloatValue(Cluster->EdgeLengths[Edge.EdgeIndex])) * ReferenceWeight;
}

UPCGExHeuristicOperation* UPCGHeuristicsFactoryShortestDistance::CreateOperation() const
{
	UPCGExHeuristicDistance* NewOperation = NewObject<UPCGExHeuristicDistance>();
	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExHeuristicsShortestDistanceProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGHeuristicsFactoryShortestDistance* NewHeuristics = NewObject<UPCGHeuristicsFactoryShortestDistance>();
	NewHeuristics->WeightFactor = Descriptor.WeightFactor;
	return Super::CreateFactory(InContext, NewHeuristics);
}
