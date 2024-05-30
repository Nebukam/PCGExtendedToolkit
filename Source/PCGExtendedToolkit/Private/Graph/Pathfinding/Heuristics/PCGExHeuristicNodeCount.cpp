// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicNodeCount.h"

double UPCGExHeuristicNodeCount::GetEdgeScore(const PCGExCluster::FNode& From, const PCGExCluster::FNode& To, const PCGExGraph::FIndexedEdge& Edge, const PCGExCluster::FNode& Seed, const PCGExCluster::FNode& Goal) const
{
	return ReferenceWeight;
}

UPCGExHeuristicOperation* UPCGHeuristicsFactoryLeastNodes::CreateOperation() const
{
	UPCGExHeuristicNodeCount* NewOperation = NewObject<UPCGExHeuristicNodeCount>();
	PCGEX_FORWARD_HEURISTIC_DESCRIPTOR
	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExHeuristicsLeastNodesProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGHeuristicsFactoryLeastNodes* NewFactory = NewObject<UPCGHeuristicsFactoryLeastNodes>();
	PCGEX_FORWARD_HEURISTIC_FACTORY
	return Super::CreateFactory(InContext, NewFactory);
}

#if WITH_EDITOR
FString UPCGExHeuristicsLeastNodesProviderSettings::GetDisplayName() const
{
	return GetDefaultNodeName().ToString()
		+ TEXT(" @ ")
		+ FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Descriptor.WeightFactor) / 1000.0));
}
#endif
