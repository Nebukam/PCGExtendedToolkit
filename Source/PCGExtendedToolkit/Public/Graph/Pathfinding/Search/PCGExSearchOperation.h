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

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual void PrepareForCluster(PCGExCluster::FCluster* InCluster);
	virtual bool FindPath(
		const FVector& SeedPosition,
		const FPCGExNodeSelectionDetails* SeedSelection,
		const FVector& GoalPosition,
		const FPCGExNodeSelectionDetails* GoalSelection,
		PCGExHeuristics::THeuristicsHandler* Heuristics,
		TArray<int32>& OutPath,
		PCGExHeuristics::FLocalFeedbackHandler* LocalFeedback = nullptr) const;
};
