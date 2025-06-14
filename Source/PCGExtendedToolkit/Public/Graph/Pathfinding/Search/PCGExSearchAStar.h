// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSearchOperation.h"


#include "UObject/Object.h"
#include "PCGExSearchAStar.generated.h"

class FPCGExHeuristicOperation;

class FPCGExSearchOperationAStar : public FPCGExSearchOperation
{
public:
	virtual bool ResolveQuery(
		const TSharedPtr<PCGExPathfinding::FPathQuery>& InQuery,
		const TSharedPtr<PCGExHeuristics::FHeuristicsHandler>& Heuristics,
		const TSharedPtr<PCGExHeuristics::FLocalFeedbackHandler>& LocalFeedback = nullptr) const override;
};

/**
 * 
 */
UCLASS(MinimalAPI, meta=(DisplayName = "A*", ToolTip ="A* Search. Returns early with the least possible amount of traversed nodes.", PCGExNodeLibraryDoc="pathfinding/search-algorithms/a-a-star"))
class UPCGExSearchAStar : public UPCGExSearchInstancedFactory
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<FPCGExSearchOperation> CreateOperation() const override
	{
		PCGEX_FACTORY_NEW_OPERATION(SearchOperationAStar)
		return NewOperation;
	}
};
