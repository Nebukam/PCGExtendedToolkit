// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExValencySolverOperation.h"
#include "Core/PCGExValencyLog.h"

void UPCGExValencySolverInstancedFactory::CopySettingsFrom(const UPCGExInstancedFactory* Other)
{
	Super::CopySettingsFrom(Other);
}

namespace PCGExValency
{
	void FDistributionTracker::Initialize(const FPCGExValencyBondingRulesCompiled* CompiledBondingRules)
	{
		if (!CompiledBondingRules)
		{
			return;
		}

		SpawnCounts.SetNumZeroed(CompiledBondingRules->ModuleCount);
		ModulesNeedingMinimum.Empty();
		ModulesAtMaximum.Empty();

		// Identify modules with minimum spawn requirements
		for (int32 i = 0; i < CompiledBondingRules->ModuleCount; ++i)
		{
			if (CompiledBondingRules->ModuleMinSpawns[i] > 0)
			{
				ModulesNeedingMinimum.Add(i);
			}
		}
	}

	bool FDistributionTracker::RecordSpawn(int32 ModuleIndex, const FPCGExValencyBondingRulesCompiled* CompiledBondingRules)
	{
		if (!CompiledBondingRules || !SpawnCounts.IsValidIndex(ModuleIndex))
		{
			return false;
		}

		// Check if already at maximum
		const int32 MaxSpawns = CompiledBondingRules->ModuleMaxSpawns[ModuleIndex];
		if (MaxSpawns >= 0 && SpawnCounts[ModuleIndex] >= MaxSpawns)
		{
			return false;
		}

		// Record spawn
		SpawnCounts[ModuleIndex]++;

		// Check if minimum is now satisfied
		const int32 MinSpawns = CompiledBondingRules->ModuleMinSpawns[ModuleIndex];
		if (SpawnCounts[ModuleIndex] >= MinSpawns)
		{
			ModulesNeedingMinimum.Remove(ModuleIndex);
		}

		// Check if maximum is now reached
		if (MaxSpawns >= 0 && SpawnCounts[ModuleIndex] >= MaxSpawns)
		{
			ModulesAtMaximum.Add(ModuleIndex);
		}

		return true;
	}

	bool FDistributionTracker::CanSpawn(int32 ModuleIndex) const
	{
		return !ModulesAtMaximum.Contains(ModuleIndex);
	}

	void FSlotBudget::Initialize(
		const FPCGExValencyBondingRulesCompiled* Rules,
		const TArray<FValencyState>& States,
		FModuleFitChecker FitChecker)
	{
		if (!Rules)
		{
			return;
		}

		// Initialize available slots per module
		AvailableSlots.SetNumZeroed(Rules->ModuleCount);

		// Initialize state-to-modules mapping
		StateToFittingModules.SetNum(States.Num());

		// For each state, find which modules fit
		for (int32 StateIndex = 0; StateIndex < States.Num(); ++StateIndex)
		{
			const FValencyState& State = States[StateIndex];
			TArray<int32>& FittingModules = StateToFittingModules[StateIndex];
			FittingModules.Empty();

			// Skip already resolved states (boundaries)
			if (State.IsResolved())
			{
				continue;
			}

			// Find all modules that fit this state
			for (int32 ModuleIndex = 0; ModuleIndex < Rules->ModuleCount; ++ModuleIndex)
			{
				if (FitChecker(ModuleIndex, State))
				{
					FittingModules.Add(ModuleIndex);
					AvailableSlots[ModuleIndex]++;
				}
			}
		}

		PCGEX_VALENCY_VERBOSE(Solver, "SlotBudget initialized: %d modules, %d states", Rules->ModuleCount, States.Num());
		for (int32 i = 0; i < Rules->ModuleCount; ++i)
		{
			if (Rules->ModuleMinSpawns[i] > 0)
			{
				PCGEX_VALENCY_VERBOSE(Solver, "  Module[%d]: MinSpawns=%d, AvailableSlots=%d",
					i, Rules->ModuleMinSpawns[i], AvailableSlots[i]);
			}
		}
	}

	void FSlotBudget::OnStateCollapsed(int32 StateIndex)
	{
		if (!StateToFittingModules.IsValidIndex(StateIndex))
		{
			return;
		}

		// Decrement available slots for all modules that could have fit this state
		for (int32 ModuleIndex : StateToFittingModules[StateIndex])
		{
			if (AvailableSlots.IsValidIndex(ModuleIndex) && AvailableSlots[ModuleIndex] > 0)
			{
				AvailableSlots[ModuleIndex]--;
			}
		}

		// Clear the fitting modules list (state is now collapsed)
		StateToFittingModules[StateIndex].Empty();
	}

	float FSlotBudget::GetUrgency(
		int32 ModuleIndex,
		const FDistributionTracker& Tracker,
		const FPCGExValencyBondingRulesCompiled* Rules) const
	{
		if (!Rules || !Rules->ModuleMinSpawns.IsValidIndex(ModuleIndex))
		{
			return 0.0f;
		}

		const int32 MinSpawns = Rules->ModuleMinSpawns[ModuleIndex];
		if (MinSpawns <= 0)
		{
			return 0.0f; // No minimum constraint
		}

		const int32 CurrentSpawns = Tracker.SpawnCounts.IsValidIndex(ModuleIndex) ? Tracker.SpawnCounts[ModuleIndex] : 0;
		const int32 RequiredSpawns = MinSpawns - CurrentSpawns;

		if (RequiredSpawns <= 0)
		{
			return 0.0f; // Minimum already satisfied
		}

		const int32 Available = AvailableSlots.IsValidIndex(ModuleIndex) ? AvailableSlots[ModuleIndex] : 0;

		if (Available <= 0)
		{
			return TNumericLimits<float>::Max(); // Impossible - no slots left but still need spawns
		}

		// Urgency = required / available
		// 0.5 = need half the remaining slots
		// 1.0 = need ALL remaining slots (must select now)
		// >1.0 = impossible (need more than available)
		return static_cast<float>(RequiredSpawns) / static_cast<float>(Available);
	}

	int32 FSlotBudget::GetForcedSelection(
		const TArray<int32>& Candidates,
		const FDistributionTracker& Tracker,
		const FPCGExValencyBondingRulesCompiled* Rules) const
	{
		int32 MostUrgentModule = -1;
		float HighestUrgency = 0.0f;

		for (int32 ModuleIndex : Candidates)
		{
			const float Urgency = GetUrgency(ModuleIndex, Tracker, Rules);

			if (Urgency >= 1.0f && Urgency > HighestUrgency)
			{
				HighestUrgency = Urgency;
				MostUrgentModule = ModuleIndex;
			}
		}

		return MostUrgentModule;
	}

	bool FSlotBudget::AreConstraintsSatisfiable(
		const FDistributionTracker& Tracker,
		const FPCGExValencyBondingRulesCompiled* Rules) const
	{
		if (!Rules)
		{
			return true;
		}

		for (int32 ModuleIndex : Tracker.ModulesNeedingMinimum)
		{
			const float Urgency = GetUrgency(ModuleIndex, Tracker, Rules);
			if (Urgency > 1.0f)
			{
				// This module needs more spawns than available slots - impossible
				return false;
			}
		}

		return true;
	}
}

void FPCGExValencySolverOperation::Initialize(
	const FPCGExValencyBondingRulesCompiled* InCompiledBondingRules,
	TArray<PCGExValency::FValencyState>& InValencyStates,
	int32 InSeed)
{
	CompiledBondingRules = InCompiledBondingRules;
	ValencyStates = &InValencyStates;
	RandomStream.Initialize(InSeed);

	DistributionTracker.Initialize(CompiledBondingRules);

	// Mark boundary states (no orbitals = NULL_SLOT)
	for (PCGExValency::FValencyState& State : *ValencyStates)
	{
		if (!State.HasOrbitals())
		{
			State.ResolvedModule = PCGExValency::SlotState::NULL_SLOT;
		}
	}
}

bool FPCGExValencySolverOperation::IsModuleCompatibleWithNeighbor(int32 ModuleIndex, int32 OrbitalIndex, int32 NeighborModuleIndex) const
{
	if (!CompiledBondingRules || CompiledBondingRules->Layers.Num() == 0)
	{
		return false;
	}

	// Check first layer (primary compatibility)
	const FPCGExValencyLayerCompiled& Layer = CompiledBondingRules->Layers[0];

	// First try the explicit neighbor list
	if (Layer.OrbitalAcceptsNeighbor(ModuleIndex, OrbitalIndex, NeighborModuleIndex))
	{
		return true;
	}

	// If orbital has no defined neighbors, check if it's a wildcard or boundary:
	// - Null cage connection (boundary orbital) = must have NO neighbor → return false
	// - No connection at all (wildcard) = accepts ANY neighbor → return true
	const int32 HeaderIndex = ModuleIndex * Layer.OrbitalCount + OrbitalIndex;
	if (Layer.NeighborHeaders.IsValidIndex(HeaderIndex))
	{
		const FIntPoint& Header = Layer.NeighborHeaders[HeaderIndex];
		if (Header.Y == 0) // Empty neighbor list
		{
			// Check if this is a boundary orbital (connected to null cage)
			const int64 BoundaryMask = CompiledBondingRules->GetModuleBoundaryMask(ModuleIndex, 0);
			const bool bIsBoundary = (BoundaryMask & (1LL << OrbitalIndex)) != 0;

			// If NOT a boundary, it's a wildcard - accept any neighbor
			if (!bIsBoundary)
			{
				return true;
			}
		}
	}

	return false;
}

bool FPCGExValencySolverOperation::DoesModuleFitState(int32 ModuleIndex, const PCGExValency::FValencyState& State) const
{
	if (!CompiledBondingRules)
	{
		return false;
	}

	// Check all layers
	for (int32 LayerIndex = 0; LayerIndex < CompiledBondingRules->GetLayerCount(); ++LayerIndex)
	{
		const int64 ModuleMask = CompiledBondingRules->GetModuleOrbitalMask(ModuleIndex, LayerIndex);
		const int64 BoundaryMask = CompiledBondingRules->GetModuleBoundaryMask(ModuleIndex, LayerIndex);
		const int64 StateMask = State.OrbitalMasks.IsValidIndex(LayerIndex) ? State.OrbitalMasks[LayerIndex] : 0;

		// Module's required orbitals must be present in state
		if ((ModuleMask & StateMask) != ModuleMask)
		{
			PCGEX_VALENCY_VERBOSE(Solver, "    Module[%d] REJECTED at Layer[%d]: ModuleMask=0x%llX, StateMask=0x%llX, (ModuleMask & StateMask)=0x%llX != ModuleMask",
				ModuleIndex, LayerIndex, ModuleMask, StateMask, (ModuleMask & StateMask));
			return false;
		}

		// Module's boundary orbitals must NOT have connections in state
		// (BoundaryMask has bits set for orbitals that must be empty; StateMask has bits set for orbitals with neighbors)
		if ((BoundaryMask & StateMask) != 0)
		{
			PCGEX_VALENCY_VERBOSE(Solver, "    Module[%d] REJECTED at Layer[%d]: BoundaryMask=0x%llX conflicts with StateMask=0x%llX",
				ModuleIndex, LayerIndex, BoundaryMask, StateMask);
			return false;
		}
	}

	return true;
}

int32 FPCGExValencySolverOperation::SelectWeightedRandom(const TArray<int32>& Candidates)
{
	if (Candidates.Num() == 0)
	{
		return -1;
	}

	if (Candidates.Num() == 1)
	{
		return Candidates[0];
	}

	// Calculate total weight
	float TotalWeight = 0.0f;
	TArray<float> CumulativeWeights;
	CumulativeWeights.Reserve(Candidates.Num());

	// Prioritize modules that need more spawns to meet minimum
	const TSet<int32>& NeedingMinimum = DistributionTracker.GetModulesNeedingMinimum();

	for (int32 ModuleIndex : Candidates)
	{
		float Weight = CompiledBondingRules->ModuleWeights[ModuleIndex];

		// Boost weight for modules needing minimum spawns
		if (NeedingMinimum.Contains(ModuleIndex))
		{
			Weight *= MinimumSpawnWeightBoost;
		}

		TotalWeight += Weight;
		CumulativeWeights.Add(TotalWeight);
	}

	if (TotalWeight <= 0.0f)
	{
		// Fallback to uniform random if weights are bad
		return Candidates[RandomStream.RandRange(0, Candidates.Num() - 1)];
	}

	// Weighted random selection
	const float RandomValue = RandomStream.FRand() * TotalWeight;

	for (int32 i = 0; i < CumulativeWeights.Num(); ++i)
	{
		if (RandomValue <= CumulativeWeights[i])
		{
			return Candidates[i];
		}
	}

	// Fallback (shouldn't reach here)
	return Candidates.Last();
}
