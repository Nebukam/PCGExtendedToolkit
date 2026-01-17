// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExValenceSolverOperation.h"

#include "PCGExValenceEntropySolver.generated.h"

/**
 * WFC-specific per-slot state (internal to entropy solver).
 * This is separate from FNodeSlot to keep solver-specific data isolated.
 */
struct FWFCSlotState
{
	/** Valid candidate module indices (shrinks during propagation) */
	TArray<int32> Candidates;

	/** Cached entropy value for priority queue ordering */
	float Entropy = TNumericLimits<float>::Max();

	/** Ratio of resolved neighbors (higher = more constrained, used as tiebreaker) */
	float NeighborResolutionRatio = 0.0f;

	/** Reset state */
	void Reset()
	{
		Candidates.Empty();
		Entropy = TNumericLimits<float>::Max();
		NeighborResolutionRatio = 0.0f;
	}
};

/**
 * Entropy-based WFC solver.
 * Collapses slots in order of lowest entropy (fewest candidates).
 * Uses neighbor resolution ratio as tiebreaker.
 */
class PCGEXELEMENTSVALENCE_API FPCGExValenceEntropySolver : public FPCGExValenceSolverOperation
{
public:
	FPCGExValenceEntropySolver() = default;
	virtual ~FPCGExValenceEntropySolver() override = default;

	/** Weight boost multiplier for modules that need more spawns to meet minimum */
	float MinimumSpawnWeightBoost = 2.0f;

	virtual void Initialize(
		const UPCGExValenceRulesetCompiled* InCompiledRuleset,
		TArray<PCGExValence::FNodeSlot>& InNodeSlots,
		int32 InSeed) override;

	virtual PCGExValence::FSolveResult Solve() override;

protected:
	/** WFC-specific state per slot (parallel to NodeSlots) */
	TArray<FWFCSlotState> SlotStates;

	/** Priority queue of unresolved slot indices, sorted by entropy */
	TArray<int32> EntropyQueue;

	/**
	 * Initialize candidates for all slots based on socket mask matching.
	 */
	void InitializeAllCandidates();

	/**
	 * Calculate entropy for a slot.
	 * Entropy = candidate count - (tiebreaker based on neighbor resolution)
	 */
	void UpdateEntropy(int32 SlotIndex);

	/**
	 * Rebuild the entropy queue from all unresolved slots.
	 */
	void RebuildEntropyQueue();

	/**
	 * Pop the lowest entropy slot from the queue.
	 * @return Slot index, or -1 if queue is empty
	 */
	int32 PopLowestEntropy();

	/**
	 * Collapse a slot by selecting a module from its candidates.
	 * @param SlotIndex The slot to collapse
	 * @return True if successful, false if no valid candidates (contradiction)
	 */
	bool CollapseSlot(int32 SlotIndex);

	/**
	 * Propagate constraints from a resolved slot to its neighbors.
	 * Updates neighbor entropy values.
	 * @param ResolvedSlotIndex The just-resolved slot
	 */
	void PropagateConstraints(int32 ResolvedSlotIndex);

	/**
	 * Filter candidates for a slot based on resolved neighbor constraints.
	 * @param SlotIndex The slot to filter
	 * @return True if candidates remain, false if empty (contradiction)
	 */
	bool FilterCandidates(int32 SlotIndex);

	/**
	 * Select a module from candidates using weighted random.
	 * Considers distribution constraints (prefer modules needing minimum).
	 */
	int32 SelectWeightedRandom(const TArray<int32>& Candidates);
};

/**
 * Factory for entropy-based WFC solver.
 */
UCLASS(DisplayName = "Entropy Solver", meta=(ToolTip = "Entropy-based WFC solver. Collapses slots with fewest candidates first.", PCGExNodeLibraryDoc="valence/solvers/entropy"))
class PCGEXELEMENTSVALENCE_API UPCGExValenceEntropySolver : public UPCGExValenceSolverInstancedFactory
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<FPCGExValenceSolverOperation> CreateOperation() const override
	{
		PCGEX_FACTORY_NEW_OPERATION(ValenceEntropySolver)
		NewOperation->MinimumSpawnWeightBoost = MinimumSpawnWeightBoost;
		return NewOperation;
	}

	/** Weight boost multiplier for modules that need more spawns to meet minimum */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin="1.0"))
	float MinimumSpawnWeightBoost = 2.0f;
};
