// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Factories/PCGExInstancedFactory.h"
#include "Factories/PCGExOperation.h"
#include "PCGExValencyCommon.h"
#include "PCGExValencyBondingRules.h"

#include "PCGExValencySolverOperation.generated.h"

namespace PCGExValency
{
	/**
	 * Minimal per-node data for solver input/output.
	 * Solvers read orbital info and write ResolvedModule.
	 * Solver-specific state (candidates, entropy, etc.) lives in the solver.
	 */
	struct PCGEXELEMENTSVALENCY_API FValencyState
	{
		/** Index in the cluster (node-space, not point-space) */
		int32 NodeIndex = -1;

		/** Orbital masks per layer (cached from buffer in node-order) */
		TArray<int64> OrbitalMasks;

		/** Neighbor node index per orbital bit position. -1 = no neighbor (boundary) */
		TArray<int32> OrbitalToNeighbor;

		/** Output: resolved module index, or special SlotState value */
		int32 ResolvedModule = SlotState::UNSET;

		/** Check if this state has been resolved (success, boundary, or unsolvable) */
		bool IsResolved() const { return ResolvedModule >= 0 || ResolvedModule == SlotState::NULL_SLOT || ResolvedModule == SlotState::UNSOLVABLE; }

		/** Check if this is a boundary state (no orbitals, marked NULL) */
		bool IsBoundary() const { return ResolvedModule == SlotState::NULL_SLOT; }

		/** Check if this state failed to solve (contradiction) */
		bool IsUnsolvable() const { return ResolvedModule == SlotState::UNSOLVABLE; }

		/** Get neighbor count */
		int32 GetNeighborCount() const
		{
			int32 Count = 0;
			for (int32 N : OrbitalToNeighbor) { if (N >= 0) { ++Count; } }
			return Count;
		}

		/** Check if state has any orbitals */
		bool HasOrbitals() const
		{
			for (int64 Mask : OrbitalMasks) { if (Mask != 0) { return true; } }
			return false;
		}
	};

	/**
	 * Distribution constraint tracker for min/max spawn counts.
	 */
	struct PCGEXELEMENTSVALENCY_API FDistributionTracker
	{
		/** Current spawn count per module index */
		TArray<int32> SpawnCounts;

		/** Modules that still need more spawns to meet minimum */
		TSet<int32> ModulesNeedingMinimum;

		/** Modules that have reached their maximum */
		TSet<int32> ModulesAtMaximum;

		/** Initialize from compiled bonding rules */
		void Initialize(const FPCGExValencyBondingRulesCompiled* CompiledBondingRules);

		/** Record a module spawn, update constraints. Returns false if would exceed max. */
		bool RecordSpawn(int32 ModuleIndex, const FPCGExValencyBondingRulesCompiled* CompiledBondingRules);

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
	struct PCGEXELEMENTSVALENCY_API FSolveResult
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
 * Base class for Valency solver operations.
 * Derive from this to create custom solving algorithms (WFC, Chemistry, etc.).
 *
 * Solvers receive ValencyStates with input data (orbital masks, neighbor mapping)
 * and must write ResolvedModule to each state.
 */
class PCGEXELEMENTSVALENCY_API FPCGExValencySolverOperation : public FPCGExOperation
{
public:
	FPCGExValencySolverOperation() = default;
	virtual ~FPCGExValencySolverOperation() override = default;

	/**
	 * Initialize the solver with bonding rules and states.
	 * Override to set up solver-specific state.
	 * @param InCompiledBondingRules The compiled bonding rules with module/orbital definitions
	 * @param InValencyStates Array of states (one per cluster node) - solver writes ResolvedModule
	 * @param InSeed Random seed for deterministic solving
	 */
	virtual void Initialize(
		const FPCGExValencyBondingRulesCompiled* InCompiledBondingRules,
		TArray<PCGExValency::FValencyState>& InValencyStates,
		int32 InSeed);

	/**
	 * Run the full solve algorithm.
	 * Must set ResolvedModule on each state.
	 * @return Result containing success state and statistics
	 */
	virtual PCGExValency::FSolveResult Solve() PCGEX_NOT_IMPLEMENTED_RET(Solve, PCGExValency::FSolveResult());

	/** Get the distribution tracker for inspection */
	const PCGExValency::FDistributionTracker& GetDistributionTracker() const { return DistributionTracker; }

protected:
	/** The compiled bonding rules */
	const FPCGExValencyBondingRulesCompiled* CompiledBondingRules = nullptr;

	/** Valency states (owned externally by staging node) */
	TArray<PCGExValency::FValencyState>* ValencyStates = nullptr;

	/** Distribution constraint tracker (shared utility) */
	PCGExValency::FDistributionTracker DistributionTracker;

	/** Random stream for deterministic selection */
	FRandomStream RandomStream;

	/**
	 * Check if a module is compatible with a neighbor at a specific orbital.
	 * Utility for solvers that need adjacency checking.
	 */
	bool IsModuleCompatibleWithNeighbor(int32 ModuleIndex, int32 OrbitalIndex, int32 NeighborModuleIndex) const;

	/**
	 * Check if a module's orbital mask matches a state's available orbitals.
	 * @param ModuleIndex Module to check
	 * @param State The state to check against
	 * @return True if module can fit in this state based on orbital geometry
	 */
	bool DoesModuleFitState(int32 ModuleIndex, const PCGExValency::FValencyState& State) const;
};

/**
 * Base factory for creating Valency solver operations.
 * Subclass this and override CreateOperation() to provide custom solvers.
 */
UCLASS(Abstract)
class PCGEXELEMENTSVALENCY_API UPCGExValencySolverInstancedFactory : public UPCGExInstancedFactory
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override;

	virtual TSharedPtr<FPCGExValencySolverOperation> CreateOperation() const PCGEX_NOT_IMPLEMENTED_RET(CreateOperation, nullptr);
};
