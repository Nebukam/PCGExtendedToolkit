// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSearchOperation.h"
#include "Factories/PCGExFactoryData.h"

#include "UObject/Object.h"
#include "PCGExSearchBellmanFord.generated.h"

class FPCGExHeuristicOperation;

/**
 * Bellman-Ford Search operation.
 * Can handle negative edge weights and detects negative cycles.
 */
class FPCGExSearchOperationBellmanFord : public FPCGExSearchOperation
{
public:
	/** If true, will detect negative cycles and fail if one is found */
	bool bDetectNegativeCycles = true;

	virtual bool ResolveQuery(
		const TSharedPtr<PCGExPathfinding::FPathQuery>& InQuery,
		const TSharedPtr<PCGExPathfinding::FSearchAllocations>& Allocations,
		const TSharedPtr<PCGExHeuristics::FHandler>& Heuristics,
		const TSharedPtr<PCGExHeuristics::FLocalFeedbackHandler>& LocalFeedback = nullptr) const override;

	virtual TSharedPtr<PCGExPathfinding::FSearchAllocations> NewAllocations() const override;
};

/**
 * Bellman-Ford Search Algorithm.
 * Unlike Dijkstra and A*, can handle negative edge weights.
 * Also detects negative weight cycles.
 * Slower than A* (O(V*E) vs O(E log V)) but more robust.
 * Useful when heuristics may produce negative scores.
 */
UCLASS(MinimalAPI, meta=(DisplayName = "Bellman-Ford", ToolTip ="Bellman-Ford Search. Handles negative edge weights and detects negative cycles. Slower but more robust.", PCGExNodeLibraryDoc="pathfinding/search-algorithms/bellman-ford"))
class UPCGExSearchBellmanFord : public UPCGExSearchInstancedFactory
{
	GENERATED_BODY()

public:
	/** If enabled, the search will fail if a negative weight cycle is detected */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bDetectNegativeCycles = true;

	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override;

	virtual TSharedPtr<FPCGExSearchOperation> CreateOperation() const override
	{
		PCGEX_FACTORY_NEW_OPERATION(SearchOperationBellmanFord)
		NewOperation->bDetectNegativeCycles = bDetectNegativeCycles;
		return NewOperation;
	}
};
