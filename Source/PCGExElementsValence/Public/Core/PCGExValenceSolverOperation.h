// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Factories/PCGExInstancedFactory.h"
#include "Factories/PCGExOperation.h"
#include "PCGExValenceCommon.h"
#include "PCGExValenceRuleset.h"

#include "PCGExValenceSolverOperation.generated.h"

namespace PCGExValence
{
	/**
	 * Minimal per-node data for solver input/output.
	 * Solvers read socket info and write ResolvedModule.
	 * Solver-specific state (candidates, entropy, etc.) lives in the solver.
	 */
	struct PCGEXELEMENTSVALENCE_API FNodeSlot
	{
		/** Index in the cluster (node-space, not point-space) */
		int32 NodeIndex = -1;

		/** Socket masks per layer (cached from buffer in node-order) */
		TArray<int64> SocketMasks;

		/** Neighbor node index per socket bit position. -1 = no neighbor (boundary) */
		TArray<int32> SocketToNeighbor;

		/** Output: resolved module index, or special SlotState value */
		int32 ResolvedModule = SlotState::UNSET;

		/** Check if this slot has been resolved (success, boundary, or unsolvable) */
		bool IsResolved() const { return ResolvedModule >= 0 || ResolvedModule == SlotState::NULL_SLOT || ResolvedModule == SlotState::UNSOLVABLE; }

		/** Check if this is a boundary slot (no sockets, marked NULL) */
		bool IsBoundary() const { return ResolvedModule == SlotState::NULL_SLOT; }

		/** Check if this slot failed to solve (contradiction) */
		bool IsUnsolvable() const { return ResolvedModule == SlotState::UNSOLVABLE; }

		/** Get neighbor count */
		int32 GetNeighborCount() const
		{
			int32 Count = 0;
			for (int32 N : SocketToNeighbor) { if (N >= 0) { ++Count; } }
			return Count;
		}

		/** Check if slot has any sockets */
		bool HasSockets() const
		{
			for (int64 Mask : SocketMasks) { if (Mask != 0) { return true; } }
			return false;
		}
	};

	/**
	 * Distribution constraint tracker for min/max spawn counts.
	 */
	struct PCGEXELEMENTSVALENCE_API FDistributionTracker
	{
		/** Current spawn count per module index */
		TArray<int32> SpawnCounts;

		/** Modules that still need more spawns to meet minimum */
		TSet<int32> ModulesNeedingMinimum;

		/** Modules that have reached their maximum */
		TSet<int32> ModulesAtMaximum;

		/** Initialize from compiled ruleset */
		void Initialize(const UPCGExValenceRulesetCompiled* CompiledRuleset);

		/** Record a module spawn, update constraints. Returns false if would exceed max. */
		bool RecordSpawn(int32 ModuleIndex, const UPCGExValenceRulesetCompiled* CompiledRuleset);

		/** Check if a module can still be spawned */
		bool CanSpawn(int32 ModuleIndex) const;

		/** Check if minimum constraints are satisfied */
		bool AreMinimumsSatisfied() const { return ModulesNeedingMinimum.Num() == 0; }

		/** Get modules that must be spawned to meet minimums */
		const TSet<int32>& GetModulesNeedingMinimum() const { return ModulesNeedingMinimum; }
	};

	/**
	 * Result of a solve operation.
	 */
	struct PCGEXELEMENTSVALENCE_API FSolveResult
	{
		/** Number of successfully resolved nodes */
		int32 ResolvedCount = 0;

		/** Number of nodes marked as unsolvable (contradictions) */
		int32 UnsolvableCount = 0;

		/** Number of nodes marked as boundary/null */
		int32 BoundaryCount = 0;

		/** True if all minimum spawn constraints were satisfied */
		bool MinimumsSatisfied = true;

		/** True if solving completed without critical errors */
		bool bSuccess = false;
	};
}

/**
 * Base class for Valence solver operations.
 * Derive from this to create custom solving algorithms (WFC, Chemistry, etc.).
 *
 * Solvers receive NodeSlots with input data (socket masks, neighbor mapping)
 * and must write ResolvedModule to each slot.
 */
class PCGEXELEMENTSVALENCE_API FPCGExValenceSolverOperation : public FPCGExOperation
{
public:
	FPCGExValenceSolverOperation() = default;
	virtual ~FPCGExValenceSolverOperation() override = default;

	/**
	 * Initialize the solver with ruleset and slots.
	 * Override to set up solver-specific state.
	 * @param InCompiledRuleset The compiled ruleset with module/socket definitions
	 * @param InNodeSlots Array of slots (one per cluster node) - solver writes ResolvedModule
	 * @param InSeed Random seed for deterministic solving
	 */
	virtual void Initialize(
		const UPCGExValenceRulesetCompiled* InCompiledRuleset,
		TArray<PCGExValence::FNodeSlot>& InNodeSlots,
		int32 InSeed);

	/**
	 * Run the full solve algorithm.
	 * Must set ResolvedModule on each slot.
	 * @return Result containing success state and statistics
	 */
	virtual PCGExValence::FSolveResult Solve() PCGEX_NOT_IMPLEMENTED_RET(Solve, PCGExValence::FSolveResult());

	/** Get the distribution tracker for inspection */
	const PCGExValence::FDistributionTracker& GetDistributionTracker() const { return DistributionTracker; }

protected:
	/** The compiled ruleset */
	const UPCGExValenceRulesetCompiled* CompiledRuleset = nullptr;

	/** Node slots (owned externally by staging node) */
	TArray<PCGExValence::FNodeSlot>* NodeSlots = nullptr;

	/** Distribution constraint tracker (shared utility) */
	PCGExValence::FDistributionTracker DistributionTracker;

	/** Random stream for deterministic selection */
	FRandomStream RandomStream;

	/**
	 * Check if a module is compatible with a neighbor at a specific socket.
	 * Utility for solvers that need adjacency checking.
	 */
	bool IsModuleCompatibleWithNeighbor(int32 ModuleIndex, int32 SocketIndex, int32 NeighborModuleIndex) const;

	/**
	 * Check if a module's socket mask matches a slot's available sockets.
	 * @param ModuleIndex Module to check
	 * @param Slot The slot to check against
	 * @return True if module can fit in this slot based on socket geometry
	 */
	bool DoesModuleFitSlot(int32 ModuleIndex, const PCGExValence::FNodeSlot& Slot) const;
};

/**
 * Base factory for creating Valence solver operations.
 * Subclass this and override CreateOperation() to provide custom solvers.
 */
UCLASS(Abstract)
class PCGEXELEMENTSVALENCE_API UPCGExValenceSolverInstancedFactory : public UPCGExInstancedFactory
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override;

	virtual TSharedPtr<FPCGExValenceSolverOperation> CreateOperation() const PCGEX_NOT_IMPLEMENTED_RET(CreateOperation, nullptr);
};
