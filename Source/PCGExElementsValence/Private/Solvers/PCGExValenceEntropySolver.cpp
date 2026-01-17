// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Solvers/PCGExValenceEntropySolver.h"

#define LOCTEXT_NAMESPACE "PCGExValenceEntropySolver"
#define PCGEX_NAMESPACE ValenceEntropySolver

void FPCGExValenceEntropySolver::Initialize(
	const UPCGExValenceRulesetCompiled* InCompiledRuleset,
	TArray<PCGExValence::FNodeSlot>& InNodeSlots,
	int32 InSeed)
{
	// Call base - marks boundary slots
	FPCGExValenceSolverOperation::Initialize(InCompiledRuleset, InNodeSlots, InSeed);

	// Initialize WFC-specific state
	SlotStates.SetNum(NodeSlots->Num());
	for (FWFCSlotState& State : SlotStates)
	{
		State.Reset();
	}

	// Initialize candidates for all slots
	InitializeAllCandidates();

	// Calculate initial entropy for all slots
	for (int32 i = 0; i < NodeSlots->Num(); ++i)
	{
		UpdateEntropy(i);
	}

	// Build initial entropy queue
	RebuildEntropyQueue();
}

void FPCGExValenceEntropySolver::InitializeAllCandidates()
{
	if (!CompiledRuleset)
	{
		return;
	}

	for (int32 SlotIndex = 0; SlotIndex < NodeSlots->Num(); ++SlotIndex)
	{
		const PCGExValence::FNodeSlot& Slot = (*NodeSlots)[SlotIndex];
		FWFCSlotState& State = SlotStates[SlotIndex];

		// Skip already resolved (boundary) slots
		if (Slot.IsResolved())
		{
			continue;
		}

		State.Candidates.Empty();

		// For each module, check if it fits this slot
		for (int32 ModuleIndex = 0; ModuleIndex < CompiledRuleset->ModuleCount; ++ModuleIndex)
		{
			if (DoesModuleFitSlot(ModuleIndex, Slot))
			{
				State.Candidates.Add(ModuleIndex);
			}
		}

		// If no candidates found, mark slot as unsolvable
		if (State.Candidates.Num() == 0 && Slot.HasSockets())
		{
			(*NodeSlots)[SlotIndex].ResolvedModule = PCGExValence::SlotState::UNSOLVABLE;
		}
	}
}

void FPCGExValenceEntropySolver::UpdateEntropy(int32 SlotIndex)
{
	if (!NodeSlots->IsValidIndex(SlotIndex))
	{
		return;
	}

	const PCGExValence::FNodeSlot& Slot = (*NodeSlots)[SlotIndex];
	FWFCSlotState& State = SlotStates[SlotIndex];

	if (Slot.IsResolved())
	{
		State.Entropy = TNumericLimits<float>::Max();
		return;
	}

	// Base entropy is candidate count
	State.Entropy = static_cast<float>(State.Candidates.Num());

	// Tiebreaker: ratio of resolved neighbors (more resolved = process sooner)
	int32 ResolvedNeighbors = 0;
	int32 TotalNeighbors = 0;

	for (int32 NeighborIndex : Slot.SocketToNeighbor)
	{
		if (NeighborIndex >= 0 && NodeSlots->IsValidIndex(NeighborIndex))
		{
			TotalNeighbors++;
			if ((*NodeSlots)[NeighborIndex].IsResolved())
			{
				ResolvedNeighbors++;
			}
		}
	}

	if (TotalNeighbors > 0)
	{
		State.NeighborResolutionRatio = static_cast<float>(ResolvedNeighbors) / static_cast<float>(TotalNeighbors);
		// Subtract small amount so higher resolution ratio = lower entropy = processed sooner
		State.Entropy -= State.NeighborResolutionRatio * 0.5f;
	}
}

void FPCGExValenceEntropySolver::RebuildEntropyQueue()
{
	EntropyQueue.Empty();

	for (int32 i = 0; i < NodeSlots->Num(); ++i)
	{
		if (!(*NodeSlots)[i].IsResolved())
		{
			EntropyQueue.Add(i);
		}
	}

	// Sort by entropy (ascending)
	EntropyQueue.Sort([this](int32 A, int32 B)
	{
		return SlotStates[A].Entropy < SlotStates[B].Entropy;
	});
}

int32 FPCGExValenceEntropySolver::PopLowestEntropy()
{
	if (EntropyQueue.Num() == 0)
	{
		return -1;
	}

	// Find lowest entropy in queue (queue should be sorted, but re-check due to updates)
	int32 BestQueueIndex = 0;
	float BestEntropy = TNumericLimits<float>::Max();

	for (int32 i = 0; i < EntropyQueue.Num(); ++i)
	{
		const int32 SlotIndex = EntropyQueue[i];
		if (!(*NodeSlots)[SlotIndex].IsResolved() && SlotStates[SlotIndex].Entropy < BestEntropy)
		{
			BestEntropy = SlotStates[SlotIndex].Entropy;
			BestQueueIndex = i;
		}
	}

	const int32 Result = EntropyQueue[BestQueueIndex];
	EntropyQueue.RemoveAt(BestQueueIndex);
	return Result;
}

PCGExValence::FSolveResult FPCGExValenceEntropySolver::Solve()
{
	PCGExValence::FSolveResult Result;

	if (!CompiledRuleset || !NodeSlots)
	{
		return Result;
	}

	// Count initial boundaries
	for (const PCGExValence::FNodeSlot& Slot : *NodeSlots)
	{
		if (Slot.IsBoundary())
		{
			Result.BoundaryCount++;
		}
	}

	// Main solve loop
	while (EntropyQueue.Num() > 0)
	{
		const int32 SlotIndex = PopLowestEntropy();
		if (SlotIndex < 0)
		{
			break;
		}

		if (!CollapseSlot(SlotIndex))
		{
			// Contradiction - slot is now unsolvable but we continue with others
		}
	}

	// Count results
	for (const PCGExValence::FNodeSlot& Slot : *NodeSlots)
	{
		if (Slot.ResolvedModule >= 0)
		{
			Result.ResolvedCount++;
		}
		else if (Slot.IsUnsolvable())
		{
			Result.UnsolvableCount++;
		}
	}

	Result.MinimumsSatisfied = DistributionTracker.AreMinimumsSatisfied();
	Result.bSuccess = (Result.UnsolvableCount == 0) && Result.MinimumsSatisfied;

	return Result;
}

bool FPCGExValenceEntropySolver::CollapseSlot(int32 SlotIndex)
{
	if (!NodeSlots->IsValidIndex(SlotIndex))
	{
		return false;
	}

	PCGExValence::FNodeSlot& Slot = (*NodeSlots)[SlotIndex];
	FWFCSlotState& State = SlotStates[SlotIndex];

	// Already resolved (shouldn't happen, but safety check)
	if (Slot.IsResolved())
	{
		return true;
	}

	// Filter candidates based on current neighbor states
	if (!FilterCandidates(SlotIndex))
	{
		// No valid candidates - mark as unsolvable
		Slot.ResolvedModule = PCGExValence::SlotState::UNSOLVABLE;
		return false;
	}

	// Select module using weighted random
	const int32 SelectedModule = SelectWeightedRandom(State.Candidates);

	if (SelectedModule < 0)
	{
		Slot.ResolvedModule = PCGExValence::SlotState::UNSOLVABLE;
		return false;
	}

	// Record the selection
	Slot.ResolvedModule = SelectedModule;
	State.Candidates.Empty();
	DistributionTracker.RecordSpawn(SelectedModule, CompiledRuleset);

	// Propagate constraints to neighbors
	PropagateConstraints(SlotIndex);

	return true;
}

void FPCGExValenceEntropySolver::PropagateConstraints(int32 ResolvedSlotIndex)
{
	if (!NodeSlots->IsValidIndex(ResolvedSlotIndex))
	{
		return;
	}

	const PCGExValence::FNodeSlot& ResolvedSlot = (*NodeSlots)[ResolvedSlotIndex];

	// For each socket, notify the neighbor
	for (int32 SocketIndex = 0; SocketIndex < ResolvedSlot.SocketToNeighbor.Num(); ++SocketIndex)
	{
		const int32 NeighborIndex = ResolvedSlot.SocketToNeighbor[SocketIndex];
		if (NeighborIndex < 0 || !NodeSlots->IsValidIndex(NeighborIndex))
		{
			continue;
		}

		if ((*NodeSlots)[NeighborIndex].IsResolved())
		{
			continue;
		}

		// Update neighbor's entropy (more neighbors resolved = lower entropy)
		UpdateEntropy(NeighborIndex);
	}
}

bool FPCGExValenceEntropySolver::FilterCandidates(int32 SlotIndex)
{
	if (!CompiledRuleset || !NodeSlots->IsValidIndex(SlotIndex))
	{
		return false;
	}

	const PCGExValence::FNodeSlot& Slot = (*NodeSlots)[SlotIndex];
	FWFCSlotState& State = SlotStates[SlotIndex];

	// Filter out candidates that don't work with resolved neighbors
	for (int32 i = State.Candidates.Num() - 1; i >= 0; --i)
	{
		const int32 CandidateModule = State.Candidates[i];

		// Check distribution constraints
		if (!DistributionTracker.CanSpawn(CandidateModule))
		{
			State.Candidates.RemoveAt(i);
			continue;
		}

		// Check compatibility with each resolved neighbor
		bool bCompatible = true;
		for (int32 SocketIndex = 0; SocketIndex < Slot.SocketToNeighbor.Num() && bCompatible; ++SocketIndex)
		{
			const int32 NeighborIndex = Slot.SocketToNeighbor[SocketIndex];
			if (NeighborIndex < 0 || !NodeSlots->IsValidIndex(NeighborIndex))
			{
				continue;
			}

			const PCGExValence::FNodeSlot& NeighborSlot = (*NodeSlots)[NeighborIndex];
			if (!NeighborSlot.IsResolved() || NeighborSlot.ResolvedModule < 0)
			{
				continue; // Neighbor not resolved yet, no constraint
			}

			// Check if this candidate is compatible with the neighbor's resolved module
			if (!IsModuleCompatibleWithNeighbor(CandidateModule, SocketIndex, NeighborSlot.ResolvedModule))
			{
				bCompatible = false;
			}
		}

		if (!bCompatible)
		{
			State.Candidates.RemoveAt(i);
		}
	}

	return State.Candidates.Num() > 0;
}

int32 FPCGExValenceEntropySolver::SelectWeightedRandom(const TArray<int32>& Candidates)
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
		float Weight = CompiledRuleset->ModuleWeights[ModuleIndex];

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

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
