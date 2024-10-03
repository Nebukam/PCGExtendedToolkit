// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicNodeCount.h"

UPCGExHeuristicOperation* UPCGExHeuristicsFactoryLeastNodes::CreateOperation(FPCGExContext* InContext) const
{
	UPCGExHeuristicNodeCount* NewOperation = InContext->ManagedObjects->New<UPCGExHeuristicNodeCount>();
	PCGEX_FORWARD_HEURISTIC_CONFIG
	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExHeuristicsLeastNodesProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExHeuristicsFactoryLeastNodes* NewFactory = InContext->ManagedObjects->New<UPCGExHeuristicsFactoryLeastNodes>();
	PCGEX_FORWARD_HEURISTIC_FACTORY
	return Super::CreateFactory(InContext, NewFactory);
}

#if WITH_EDITOR
FString UPCGExHeuristicsLeastNodesProviderSettings::GetDisplayName() const
{
	return GetDefaultNodeName().ToString()
		+ TEXT(" @ ")
		+ FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.WeightFactor) / 1000.0));
}
#endif
