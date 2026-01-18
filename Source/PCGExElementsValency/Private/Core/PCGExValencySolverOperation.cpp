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
	return Layer.OrbitalAcceptsNeighbor(ModuleIndex, OrbitalIndex, NeighborModuleIndex);
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
