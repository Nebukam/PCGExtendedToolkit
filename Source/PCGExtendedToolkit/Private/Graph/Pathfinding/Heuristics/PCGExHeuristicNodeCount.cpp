// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicNodeCount.h"




double UPCGExHeuristicNodeCount::GetGlobalScore(const PCGExCluster::FNode& From, const PCGExCluster::FNode& Seed, const PCGExCluster::FNode& Goal) const
{
	return GetScoreInternal(0.5);
}

double UPCGExHeuristicNodeCount::GetEdgeScore(const PCGExCluster::FNode& From, const PCGExCluster::FNode& To, const PCGExGraph::FEdge& Edge, const PCGExCluster::FNode& Seed, const PCGExCluster::FNode& Goal, const TSharedPtr<PCGEx::FHashLookup> TravelStack) const
{
	return GetScoreInternal(0.5);
}

TSharedPtr<UPCGExHeuristicOperation> UPCGExHeuristicsFactoryLeastNodes::CreateOperation(FPCGExContext* InContext) const
{
	PCGEX_FACTORY_NEW_OPERATION(UPCGExHeuristicNodeCount)
	PCGEX_FORWARD_HEURISTIC_CONFIG
	return NewOperation;
}

PCGEX_HEURISTIC_FACTORY_BOILERPLATE_IMPL(LeastNodes, {})

UPCGExFactoryData* UPCGExHeuristicsLeastNodesProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExHeuristicsFactoryLeastNodes* NewFactory = InContext->ManagedObjects->New<UPCGExHeuristicsFactoryLeastNodes>();
	PCGEX_FORWARD_HEURISTIC_FACTORY
	return Super::CreateFactory(InContext, NewFactory);
}

#if WITH_EDITOR
FString UPCGExHeuristicsLeastNodesProviderSettings::GetDisplayName() const
{
	return GetDefaultNodeTitle().ToString().Replace(TEXT("PCGEx | Heuristics"), TEXT("HX"))
		+ TEXT(" @ ")
		+ FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.WeightFactor) / 1000.0));
}
#endif
