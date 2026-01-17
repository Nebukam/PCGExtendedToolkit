// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExValenceSolverOperation.h"

void UPCGExValenceSolverInstancedFactory::CopySettingsFrom(const UPCGExInstancedFactory* Other)
{
	Super::CopySettingsFrom(Other);
}

namespace PCGExValence
{
	void FDistributionTracker::Initialize(const UPCGExValenceRulesetCompiled* CompiledRuleset)
	{
		if (!CompiledRuleset)
		{
			return;
		}

		SpawnCounts.SetNumZeroed(CompiledRuleset->ModuleCount);
		ModulesNeedingMinimum.Empty();
		ModulesAtMaximum.Empty();

		// Identify modules with minimum spawn requirements
		for (int32 i = 0; i < CompiledRuleset->ModuleCount; ++i)
		{
			if (CompiledRuleset->ModuleMinSpawns[i] > 0)
			{
				ModulesNeedingMinimum.Add(i);
			}
		}
	}

	bool FDistributionTracker::RecordSpawn(int32 ModuleIndex, const UPCGExValenceRulesetCompiled* CompiledRuleset)
	{
		if (!CompiledRuleset || !SpawnCounts.IsValidIndex(ModuleIndex))
		{
			return false;
		}

		// Check if already at maximum
		const int32 MaxSpawns = CompiledRuleset->ModuleMaxSpawns[ModuleIndex];
		if (MaxSpawns >= 0 && SpawnCounts[ModuleIndex] >= MaxSpawns)
		{
			return false;
		}

		// Record spawn
		SpawnCounts[ModuleIndex]++;

		// Check if minimum is now satisfied
		const int32 MinSpawns = CompiledRuleset->ModuleMinSpawns[ModuleIndex];
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

void FPCGExValenceSolverOperation::Initialize(
	const UPCGExValenceRulesetCompiled* InCompiledRuleset,
	TArray<PCGExValence::FNodeSlot>& InNodeSlots,
	int32 InSeed)
{
	CompiledRuleset = InCompiledRuleset;
	NodeSlots = &InNodeSlots;
	RandomStream.Initialize(InSeed);

	DistributionTracker.Initialize(CompiledRuleset);

	// Mark boundary slots (no sockets = NULL_SLOT)
	for (PCGExValence::FNodeSlot& Slot : *NodeSlots)
	{
		if (!Slot.HasSockets())
		{
			Slot.ResolvedModule = PCGExValence::SlotState::NULL_SLOT;
		}
	}
}

bool FPCGExValenceSolverOperation::IsModuleCompatibleWithNeighbor(int32 ModuleIndex, int32 SocketIndex, int32 NeighborModuleIndex) const
{
	if (!CompiledRuleset || CompiledRuleset->Layers.Num() == 0)
	{
		return false;
	}

	// Check first layer (primary compatibility)
	const FPCGExValenceLayerCompiled& Layer = CompiledRuleset->Layers[0];
	return Layer.SocketAcceptsNeighbor(ModuleIndex, SocketIndex, NeighborModuleIndex);
}

bool FPCGExValenceSolverOperation::DoesModuleFitSlot(int32 ModuleIndex, const PCGExValence::FNodeSlot& Slot) const
{
	if (!CompiledRuleset)
	{
		return false;
	}

	// Check all layers - module's required sockets must match slot's available connections
	for (int32 LayerIndex = 0; LayerIndex < CompiledRuleset->GetLayerCount(); ++LayerIndex)
	{
		const int64 ModuleMask = CompiledRuleset->GetModuleSocketMask(ModuleIndex, LayerIndex);
		const int64 SlotMask = Slot.SocketMasks.IsValidIndex(LayerIndex) ? Slot.SocketMasks[LayerIndex] : 0;

		if ((ModuleMask & SlotMask) != ModuleMask)
		{
			return false;
		}
	}

	return true;
}
