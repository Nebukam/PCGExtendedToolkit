﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDistance.h"

void UPCGExHeuristicDistance::PrepareForCluster(PCGExCluster::FCluster* InCluster)
{
	Super::PrepareForCluster(InCluster);
	MaxDistSquared = FVector::DistSquared(InCluster->Bounds.Min, InCluster->Bounds.Max);
}

double UPCGExHeuristicDistance::GetGlobalScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return SampleCurve(FVector::DistSquared(From.Position, Goal.Position) / MaxDistSquared) * ReferenceWeight;
}

double UPCGExHeuristicDistance::GetEdgeScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& To,
	const PCGExGraph::FIndexedEdge& Edge,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return SampleCurve(Cluster->EdgeLengths[Edge.EdgeIndex]) * ReferenceWeight;
}

UPCGExHeuristicOperation* UPCGHeuristicsFactoryShortestDistance::CreateOperation() const
{
	UPCGExHeuristicDistance* NewOperation = NewObject<UPCGExHeuristicDistance>();
	PCGEX_FORWARD_HEURISTIC_DESCRIPTOR
	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExHeuristicsShortestDistanceProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGHeuristicsFactoryShortestDistance* NewFactory = NewObject<UPCGHeuristicsFactoryShortestDistance>();
	PCGEX_FORWARD_HEURISTIC_FACTORY
	return Super::CreateFactory(InContext, NewFactory);
}

#if WITH_EDITOR
FString UPCGExHeuristicsShortestDistanceProviderSettings::GetDisplayName() const
{
	return GetDefaultNodeName().ToString()
		+ TEXT(" @ ")
		+ FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Descriptor.WeightFactor) / 1000.0));
}
#endif
