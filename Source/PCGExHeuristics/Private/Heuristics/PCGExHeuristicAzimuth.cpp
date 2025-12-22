// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Heuristics/PCGExHeuristicAzimuth.h"

#include "Clusters/PCGExCluster.h"
#include "Containers/PCGExManagedObjects.h"
#include "Math/PCGExMath.h"


double FPCGExHeuristicAzimuth::GetGlobalScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal) const
{
	const FVector Dir = Cluster->GetDir(Seed, Goal);
	const double Dot = FVector::DotProduct(Dir, Cluster->GetDir(From, Goal)) * -1;
	return GetScoreInternal(PCGExMath::Remap(Dot, -1, 1, 0, 1));
}

double FPCGExHeuristicAzimuth::GetEdgeScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& To, const PCGExGraphs::FEdge& Edge, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const TSharedPtr<PCGEx::FHashLookup> TravelStack) const
{
	const double Dot = (FVector::DotProduct(Cluster->GetDir(From, To), Cluster->GetDir(From, Goal)) * -1);
	return GetScoreInternal(PCGExMath::Remap(Dot, -1, 1, 1, 0));
}

TSharedPtr<FPCGExHeuristicOperation> UPCGExHeuristicsFactoryAzimuth::CreateOperation(FPCGExContext* InContext) const
{
	PCGEX_FACTORY_NEW_OPERATION(HeuristicAzimuth)
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
	return GetDefaultNodeTitle().ToString().Replace(TEXT("PCGEx | Heuristics"), TEXT("HX")) + TEXT(" @ ") + FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.WeightFactor) / 1000.0));
}
#endif
