// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOperation.h"
#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"
#include "UObject/Object.h"
#include "PCGExSearchOperation.generated.h"

namespace PCGExPathfinding
{
	struct FExtraWeights;
}

struct FPCGExHeuristicModifiersSettings;
class UPCGExHeuristicOperation;

namespace PCGExCluster
{
	struct FCluster;
}

/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExSearchOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	PCGExCluster::FCluster* Cluster = nullptr;
	PCGExCluster::FClusterProjection* Projection = nullptr;

	virtual bool GetRequiresProjection();
	virtual void PrepareForCluster(PCGExCluster::FCluster* InCluster, PCGExCluster::FClusterProjection* InProjection = nullptr);
	virtual bool FindPath(
		const FVector& SeedPosition,
		const FPCGExNodeSelectionSettings* SeedSelection,
		const FVector& GoalPosition,
		const FPCGExNodeSelectionSettings* GoalSelection,
		PCGExHeuristics::THeuristicsHandler* Heuristics,
		TArray<int32>& OutPath,
		PCGExHeuristics::FLocalFeedbackHandler* LocalFeedback = nullptr);
};
