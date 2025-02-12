// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/Heuristics/PCGExHeuristicAzimuth.h"


void UPCGExHeuristicAzimuth::PrepareForCluster(const TSharedPtr<const PCGExCluster::FCluster>& InCluster)
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

double UPCGExHeuristicAzimuth::GetGlobalScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	const FVector Dir = Cluster->GetDir(Seed, Goal);
	const double Dot = FVector::DotProduct(Dir, Cluster->GetDir(From, Goal)) * -1;
	return GetScoreInternal(PCGExMath::Remap(Dot, -1, 1, OutMin, OutMax));
}

double UPCGExHeuristicAzimuth::GetEdgeScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& To,
	const PCGExGraph::FEdge& Edge,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal,
	const TSharedPtr<PCGEx::FHashLookup> TravelStack) const
{
	const double Dot = (FVector::DotProduct(Cluster->GetDir(From, To), Cluster->GetDir(From, Goal)) * -1);
	return GetScoreInternal(PCGExMath::Remap(Dot, -1, 1, OutMin, OutMax));
}

UPCGExHeuristicOperation* UPCGExHeuristicsFactoryAzimuth::CreateOperation(FPCGExContext* InContext) const
{
	UPCGExHeuristicAzimuth* NewOperation = InContext->ManagedObjects->New<UPCGExHeuristicAzimuth>();
	PCGEX_FORWARD_HEURISTIC_CONFIG
	return NewOperation;
}

PCGEX_HEURISTIC_FACTORY_BOILERPLATE_IMPL(Azimuth, {})

UPCGExFactoryData* UPCGExHeuristicsAzimuthProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExHeuristicsFactoryAzimuth* NewFactory = InContext->ManagedObjects->New<UPCGExHeuristicsFactoryAzimuth>();
	PCGEX_FORWARD_HEURISTIC_FACTORY
	return Super::CreateFactory(InContext, NewFactory);
}

#if WITH_EDITOR
FString UPCGExHeuristicsAzimuthProviderSettings::GetDisplayName() const
{
	return GetDefaultNodeTitle().ToString().Replace(TEXT("PCGEx | Heuristics"), TEXT("HX"))
		+ TEXT(" @ ")
		+ FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.WeightFactor) / 1000.0));
}
#endif
