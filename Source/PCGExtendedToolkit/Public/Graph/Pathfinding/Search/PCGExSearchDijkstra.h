// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSearchOperation.h"
#include "UObject/Object.h"
#include "PCGExSearchDijkstra.generated.h"

namespace PCGExHeuristics
{
	class THeuristicsHandler;
}

namespace PCGExCluster
{
	struct FCluster;
}

struct FPCGExHeuristicModifiersSettings;
class UPCGExHeuristicOperation;
/**
 * 
 */
UCLASS(DisplayName = "Dijkstra", meta=(ToolTip ="Dijkstra search. Slower than A* but more respectful of modifiers and weights."))
class PCGEXTENDEDTOOLKIT_API UPCGExSearchDijkstra : public UPCGExSearchOperation
{
	GENERATED_BODY()

public:
	virtual bool FindPath(
		const FVector& SeedPosition,
		const FPCGExNodeSelectionSettings* SeedSelection,
		const FVector& GoalPosition,
		const FPCGExNodeSelectionSettings* GoalSelection,
		PCGExHeuristics::THeuristicsHandler* Heuristics,
		TArray<int32>& OutPath, PCGExHeuristics::FLocalFeedbackHandler* LocalFeedback) override;
};
