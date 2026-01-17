// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Solvers/PCGExValencyEntropySolver.h"

#define LOCTEXT_NAMESPACE "PCGExValencyEntropySolver"
#define PCGEX_NAMESPACE ValencyEntropySolver

void FPCGExValencyEntropySolver::Initialize(
	const FPCGExValencyBondingRulesCompiled* InCompiledBondingRules,
	TArray<PCGExValency::FValencyState>& InValencyStates,
	int32 InSeed)
{
	// Call base - marks boundary states
	FPCGExValencySolverOperation::Initialize(InCompiledBondingRules, InValencyStates, InSeed);

	// Initialize WFC-specific state
	StateData.SetNum(ValencyStates->Num());
	for (FWFCStateData& Data : StateData)
	{
		Data.Reset();
	}

	// Initialize candidates for all states
	InitializeAllCandidates();

	// Calculate initial entropy for all states
	for (int32 i = 0; i < ValencyStates->Num(); ++i)
	{
		UpdateEntropy(i);
	}

	// Build initial entropy queue
	RebuildEntropyQueue();
}

void FPCGExValencyEntropySolver::InitializeAllCandidates()
{
	if (!CompiledBondingRules)
	{
		return;
	}

	for (int32 StateIndex = 0; StateIndex < ValencyStates->Num(); ++StateIndex)
	{
		const PCGExValency::FValencyState& State = (*ValencyStates)[StateIndex];
		FWFCStateData& Data = StateData[StateIndex];

		// Skip already resolved (boundary) states
		if (State.IsResolved())
		{
			continue;
		}

		Data.Candidates.Empty();

		// For each module, check if it fits this state
		for (int32 ModuleIndex = 0; ModuleIndex < CompiledBondingRules->ModuleCount; ++ModuleIndex)
		{
			if (DoesModuleFitState(ModuleIndex, State))
			{
				Data.Candidates.Add(ModuleIndex);
			}
		}

		// If no candidates found, mark state as unsolvable
		if (Data.Candidates.Num() == 0 && State.HasOrbitals())
		{
			(*ValencyStates)[StateIndex].ResolvedModule = PCGExValency::SlotState::UNSOLVABLE;
		}
	}
}

void FPCGExValencyEntropySolver::UpdateEntropy(int32 StateIndex)
{
	if (!ValencyStates->IsValidIndex(StateIndex))
	{
		return;
	}

	const PCGExValency::FValencyState& State = (*ValencyStates)[StateIndex];
	FWFCStateData& Data = StateData[StateIndex];

	if (State.IsResolved())
	{
		Data.Entropy = TNumericLimits<float>::Max();
		return;
	}

	// Base entropy is candidate count
	Data.Entropy = static_cast<float>(Data.Candidates.Num());

	// Tiebreaker: ratio of resolved neighbors (more resolved = process sooner)
	int32 ResolvedNeighbors = 0;
	int32 TotalNeighbors = 0;

	for (int32 NeighborIndex : State.OrbitalToNeighbor)
	{
		if (NeighborIndex >= 0 && ValencyStates->IsValidIndex(NeighborIndex))
		{
			TotalNeighbors++;
			if ((*ValencyStates)[NeighborIndex].IsResolved())
			{
				ResolvedNeighbors++;
			}
		}
	}

	if (TotalNeighbors > 0)
	{
		Data.NeighborResolutionRatio = static_cast<float>(ResolvedNeighbors) / static_cast<float>(TotalNeighbors);
		// Subtract small amount so higher resolution ratio = lower entropy = processed sooner
		Data.Entropy -= Data.NeighborResolutionRatio * 0.5f;
	}
}

void FPCGExValencyEntropySolver::RebuildEntropyQueue()
{
	EntropyQueue.Empty();

	for (int32 i = 0; i < ValencyStates->Num(); ++i)
	{
		if (!(*ValencyStates)[i].IsResolved())
		{
			EntropyQueue.Add(i);
		}
	}

	// Sort by entropy (ascending)
	EntropyQueue.Sort([this](int32 A, int32 B)
	{
		return StateData[A].Entropy < StateData[B].Entropy;
	});
}

int32 FPCGExValencyEntropySolver::PopLowestEntropy()
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
		const int32 StateIndex = EntropyQueue[i];
		if (!(*ValencyStates)[StateIndex].IsResolved() && StateData[StateIndex].Entropy < BestEntropy)
		{
			BestEntropy = StateData[StateIndex].Entropy;
			BestQueueIndex = i;
		}
	}

	const int32 Result = EntropyQueue[BestQueueIndex];
	EntropyQueue.RemoveAt(BestQueueIndex);
	return Result;
}

PCGExValency::FSolveResult FPCGExValencyEntropySolver::Solve()
{
	PCGExValency::FSolveResult Result;

	if (!CompiledBondingRules || !ValencyStates)
	{
		return Result;
	}

	// Count initial boundaries
	for (const PCGExValency::FValencyState& State : *ValencyStates)
	{
		if (State.IsBoundary())
		{
			Result.BoundaryCount++;
		}
	}

	// Main solve loop
	while (EntropyQueue.Num() > 0)
	{
		const int32 StateIndex = PopLowestEntropy();
		if (StateIndex < 0)
		{
			break;
		}

		if (!CollapseState(StateIndex))
		{
			// Contradiction - state is now unsolvable but we continue with others
		}
	}

	// Count results
	for (const PCGExValency::FValencyState& State : *ValencyStates)
	{
		if (State.ResolvedModule >= 0)
		{
			Result.ResolvedCount++;
		}
		else if (State.IsUnsolvable())
		{
			Result.UnsolvableCount++;
		}
	}

	Result.MinimumsSatisfied = DistributionTracker.AreMinimumsSatisfied();
	Result.bSuccess = (Result.UnsolvableCount == 0) && Result.MinimumsSatisfied;

	return Result;
}

bool FPCGExValencyEntropySolver::CollapseState(int32 StateIndex)
{
	if (!ValencyStates->IsValidIndex(StateIndex))
	{
		return false;
	}

	PCGExValency::FValencyState& State = (*ValencyStates)[StateIndex];
	FWFCStateData& Data = StateData[StateIndex];

	// Already resolved (shouldn't happen, but safety check)
	if (State.IsResolved())
	{
		return true;
	}

	// Filter candidates based on current neighbor states
	if (!FilterCandidates(StateIndex))
	{
		// No valid candidates - mark as unsolvable
		State.ResolvedModule = PCGExValency::SlotState::UNSOLVABLE;
		return false;
	}

	// Select module using weighted random
	const int32 SelectedModule = SelectWeightedRandom(Data.Candidates);

	if (SelectedModule < 0)
	{
		State.ResolvedModule = PCGExValency::SlotState::UNSOLVABLE;
		return false;
	}

	// Record the selection
	State.ResolvedModule = SelectedModule;
	Data.Candidates.Empty();
	DistributionTracker.RecordSpawn(SelectedModule, CompiledBondingRules);

	// Propagate constraints to neighbors
	PropagateConstraints(StateIndex);

	return true;
}

void FPCGExValencyEntropySolver::PropagateConstraints(int32 ResolvedStateIndex)
{
	if (!ValencyStates->IsValidIndex(ResolvedStateIndex))
	{
		return;
	}

	const PCGExValency::FValencyState& ResolvedState = (*ValencyStates)[ResolvedStateIndex];

	// For each orbital, notify the neighbor
	for (int32 OrbitalIndex = 0; OrbitalIndex < ResolvedState.OrbitalToNeighbor.Num(); ++OrbitalIndex)
	{
		const int32 NeighborIndex = ResolvedState.OrbitalToNeighbor[OrbitalIndex];
		if (NeighborIndex < 0 || !ValencyStates->IsValidIndex(NeighborIndex))
		{
			continue;
		}

		if ((*ValencyStates)[NeighborIndex].IsResolved())
		{
			continue;
		}

		// Update neighbor's entropy (more neighbors resolved = lower entropy)
		UpdateEntropy(NeighborIndex);
	}
}

bool FPCGExValencyEntropySolver::FilterCandidates(int32 StateIndex)
{
	if (!CompiledBondingRules || !ValencyStates->IsValidIndex(StateIndex))
	{
		return false;
	}

	const PCGExValency::FValencyState& State = (*ValencyStates)[StateIndex];
	FWFCStateData& Data = StateData[StateIndex];

	// Filter out candidates that don't work with resolved neighbors
	for (int32 i = Data.Candidates.Num() - 1; i >= 0; --i)
	{
		const int32 CandidateModule = Data.Candidates[i];

		// Check distribution constraints
		if (!DistributionTracker.CanSpawn(CandidateModule))
		{
			Data.Candidates.RemoveAt(i);
			continue;
		}

		// Check compatibility with each resolved neighbor
		bool bCompatible = true;
		for (int32 OrbitalIndex = 0; OrbitalIndex < State.OrbitalToNeighbor.Num() && bCompatible; ++OrbitalIndex)
		{
			const int32 NeighborIndex = State.OrbitalToNeighbor[OrbitalIndex];
			if (NeighborIndex < 0 || !ValencyStates->IsValidIndex(NeighborIndex))
			{
				continue;
			}

			const PCGExValency::FValencyState& NeighborState = (*ValencyStates)[NeighborIndex];
			if (!NeighborState.IsResolved() || NeighborState.ResolvedModule < 0)
			{
				continue; // Neighbor not resolved yet, no constraint
			}

			// Check if this candidate is compatible with the neighbor's resolved module
			if (!IsModuleCompatibleWithNeighbor(CandidateModule, OrbitalIndex, NeighborState.ResolvedModule))
			{
				bCompatible = false;
			}
		}

		if (!bCompatible)
		{
			Data.Candidates.RemoveAt(i);
		}
	}

	return Data.Candidates.Num() > 0;
}

int32 FPCGExValencyEntropySolver::SelectWeightedRandom(const TArray<int32>& Candidates)
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

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
