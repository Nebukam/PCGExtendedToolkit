// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSearchOperation.h"
#include "Factories/PCGExFactoryData.h"
#include "UObject/Object.h"
#include "PCGExSearchDijkstra.generated.h"

class FPCGExHeuristicOperation;

class FPCGExSearchOperationDijkstra : public FPCGExSearchOperation
{
public:
	virtual bool ResolveQuery(
		const TSharedPtr<PCGExPathfinding::FPathQuery>& InQuery,
		const TSharedPtr<PCGExPathfinding::FSearchAllocations>& Allocations,
		const TSharedPtr<PCGExHeuristics::FHandler>& Heuristics,
		const TSharedPtr<PCGExHeuristics::FLocalFeedbackHandler>& LocalFeedback = nullptr) const override;
};

/**
 * 
 */
UCLASS(MinimalAPI, meta=(DisplayName = "Dijkstra", ToolTip ="Dijkstra search. Slower than A* but more respectful of modifiers and weights.", PCGExNodeLibraryDoc="pathfinding/search-algorithms/dijkstra"))
class UPCGExSearchDijkstra : public UPCGExSearchInstancedFactory
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<FPCGExSearchOperation> CreateOperation() const override
	{
		PCGEX_FACTORY_NEW_OPERATION(SearchOperationDijkstra)
		return NewOperation;
	}
};
