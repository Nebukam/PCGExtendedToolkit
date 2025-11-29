// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/Heuristics/PCGExHeuristicInertia.h"

#include "PCGExMath.h"
#include "Graph/PCGExCluster.h"


double FPCGExHeuristicInertia::GetGlobalScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return GetScoreInternal(GlobalInertiaScore);
}

double FPCGExHeuristicInertia::GetEdgeScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& To,
	const PCGExGraph::FEdge& Edge,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal,
	const TSharedPtr<PCGEx::FHashLookup> TravelStack) const
{
	if (TravelStack)
	{
		int32 PathNodeIndex = PCGEx::NH64A(TravelStack->Get(From.Index));
		int32 PathEdgeIndex = -1;

		if (PathNodeIndex != -1)
		{
			FVector Avg = Cluster->GetDir(PathNodeIndex, From.Index);
			int32 Sampled = 1;
			while (PathNodeIndex != -1 && Sampled < MaxSamples)
			{
				const int32 CurrentIndex = PathNodeIndex;
				PCGEx::NH64(TravelStack->Get(CurrentIndex), PathNodeIndex, PathEdgeIndex);
				if (PathNodeIndex != -1)
				{
					Avg += Cluster->GetDir(PathNodeIndex, CurrentIndex);
					Sampled++;
				}
			}

			if (!bIgnoreIfNotEnoughSamples || Sampled == MaxSamples)
			{
				const double Dot = FVector::DotProduct(
					(Avg / Sampled).GetSafeNormal(),
					Cluster->GetDir(From.Index, To.Index));

				return GetScoreInternal(PCGExMath::Remap(Dot, -1, 1, 1, 0)) * ReferenceWeight;
			}
		}
	}

	return GetScoreInternal(FallbackInertiaScore);
}

TSharedPtr<FPCGExHeuristicOperation> UPCGExHeuristicsFactoryInertia::CreateOperation(FPCGExContext* InContext) const
{
	PCGEX_FACTORY_NEW_OPERATION(HeuristicInertia)
	PCGEX_FORWARD_HEURISTIC_CONFIG
	NewOperation->GlobalInertiaScore = Config.GlobalInertiaScore;
	NewOperation->FallbackInertiaScore = Config.FallbackInertiaScore;
	NewOperation->MaxSamples = Config.Samples;
	NewOperation->bIgnoreIfNotEnoughSamples = Config.bIgnoreIfNotEnoughSamples;
	return NewOperation;
}

PCGEX_HEURISTIC_FACTORY_BOILERPLATE_IMPL(Inertia, {})

UPCGExFactoryData* UPCGExHeuristicsInertiaProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExHeuristicsFactoryInertia* NewFactory = InContext->ManagedObjects->New<UPCGExHeuristicsFactoryInertia>();
	PCGEX_FORWARD_HEURISTIC_FACTORY
	return Super::CreateFactory(InContext, NewFactory);
}

#if WITH_EDITOR
FString UPCGExHeuristicsInertiaProviderSettings::GetDisplayName() const
{
	return GetDefaultNodeTitle().ToString().Replace(TEXT("PCGEx | Heuristics"), TEXT("HX"))
		+ TEXT(" @ ")
		+ FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.WeightFactor) / 1000.0));
}
#endif
