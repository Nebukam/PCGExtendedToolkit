// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicNodeCount.h"

UPCGExHeuristicOperation* UPCGExHeuristicsFactoryLeastNodes::CreateOperation(FPCGExContext* InContext) const
{
	UPCGExHeuristicNodeCount* NewOperation = InContext->ManagedObjects->New<UPCGExHeuristicNodeCount>();
	PCGEX_FORWARD_HEURISTIC_CONFIG
	return NewOperation;
}

PCGEX_HEURISTIC_FACTORY_BOILERPLATE_IMPL(LeastNodes, {})

UPCGExParamFactoryBase* UPCGExHeuristicsLeastNodesProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
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
