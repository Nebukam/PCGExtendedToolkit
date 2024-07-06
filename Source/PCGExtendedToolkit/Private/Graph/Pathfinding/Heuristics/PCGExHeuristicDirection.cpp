// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDirection.h"

void UPCGExHeuristicDirection::PrepareForCluster(const PCGExCluster::FCluster* InCluster)
{
	if (bInvert)
	{
		OutMin = 1;
		OutMax = 0;
	}
	else
	{
		OutMin = 0;
		OutMax = 1;
	}
	Super::PrepareForCluster(InCluster);
}

UPCGExHeuristicOperation* UPCGHeuristicsFactoryDirection::CreateOperation() const
{
	UPCGExHeuristicDirection* NewOperation = NewObject<UPCGExHeuristicDirection>();
	PCGEX_FORWARD_HEURISTIC_CONFIG
	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExHeuristicsDirectionProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGHeuristicsFactoryDirection* NewFactory = NewObject<UPCGHeuristicsFactoryDirection>();
	PCGEX_FORWARD_HEURISTIC_FACTORY
	return Super::CreateFactory(InContext, NewFactory);
}

#if WITH_EDITOR
FString UPCGExHeuristicsDirectionProviderSettings::GetDisplayName() const
{
	return GetDefaultNodeName().ToString()
		+ TEXT(" @ ")
		+ FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.WeightFactor) / 1000.0));
}
#endif
