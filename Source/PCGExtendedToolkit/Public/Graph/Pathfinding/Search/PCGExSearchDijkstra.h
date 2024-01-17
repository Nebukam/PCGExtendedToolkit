// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSearchOperation.h"
#include "UObject/Object.h"
#include "PCGExSearchDijkstra.generated.h"

namespace PCGExCluster
{
	struct FCluster;
}

struct FPCGExHeuristicModifiersSettings;
class UPCGExHeuristicOperation;
/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExSearchDijkstra : public UPCGExSearchOperation
{
	GENERATED_BODY()

public:
	virtual bool FindPath(
		const PCGExCluster::FCluster* Cluster,
		const FVector& SeedPosition,
		const FVector& GoalPosition,
		const UPCGExHeuristicOperation* Heuristics,
		const FPCGExHeuristicModifiersSettings* Modifiers,
		TArray<int32>& OutPath) override;
};
