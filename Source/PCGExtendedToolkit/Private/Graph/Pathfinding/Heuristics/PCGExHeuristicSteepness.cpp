// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicSteepness.h"

void UPCGExHeuristicSteepness::PrepareForCluster(const PCGExCluster::FCluster* InCluster)
{
	UpwardVector = UpwardVector.GetSafeNormal();
	Super::PrepareForCluster(InCluster);
}

UPCGExHeuristicOperation* UPCGExHeuristicsFactorySteepness::CreateOperation() const
{
	PCGEX_NEW_TRANSIENT(UPCGExHeuristicSteepness, NewOperation)
	PCGEX_FORWARD_HEURISTIC_CONFIG
	NewOperation->UpwardVector = Config.UpVector;
	NewOperation->bAbsoluteSteepness = Config.bAbsoluteSteepness;
	return NewOperation;
}

UPCGExParamFactoryBase* UPCGExHeuristicsSteepnessProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExHeuristicsFactorySteepness* NewFactory = NewObject<UPCGExHeuristicsFactorySteepness>();
	PCGEX_FORWARD_HEURISTIC_FACTORY
	return Super::CreateFactory(InContext, NewFactory);
}

#if WITH_EDITOR
FString UPCGExHeuristicsSteepnessProviderSettings::GetDisplayName() const
{
	return GetDefaultNodeName().ToString()
		+ TEXT(" @ ")
		+ FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.WeightFactor) / 1000.0));
}
#endif
