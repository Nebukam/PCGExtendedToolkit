// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Solvers/PCGExValencyEntropySolver.h"
#include "Core/PCGExValencyLog.h"

#define LOCTEXT_NAMESPACE "PCGExValencyEntropySolver"
#define PCGEX_NAMESPACE ValencyEntropySolver

void FPCGExValencyEntropySolver::Initialize(
	const FPCGExValencyBondingRulesCompiled* InCompiledBondingRules,
	TArray<PCGExValency::FValencyState>& InValencyStates,
	int32 InSeed)
{
	VALENCY_LOG_SECTION(Solver, "ENTROPY SOLVER INITIALIZE");
	PCGEX_VALENCY_INFO(Solver, "Seed: %d, States: %d, CompiledRules: %s",
		InSeed, InValencyStates.Num(), InCompiledBondingRules ? TEXT("Valid") : TEXT("NULL"));

	if (InCompiledBondingRules)
	{
		PCGEX_VALENCY_INFO(Solver, "  CompiledRules: %d modules, %d layers",
			InCompiledBondingRules->ModuleCount, InCompiledBondingRules->Layers.Num());
	}

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

	VALENCY_LOG_SECTION(Solver, "ENTROPY SOLVER INIT COMPLETE");
	PCGEX_VALENCY_INFO(Solver, "Queue size=%d", EntropyQueue.Num());
}

void FPCGExValencyEntropySolver::InitializeAllCandidates()
{
	if (!CompiledBondingRules)
	{
		PCGEX_VALENCY_ERROR(Solver, "InitializeAllCandidates: CompiledBondingRules is NULL!");
		return;
	}

	VALENCY_LOG_SUBSECTION(Solver, "Initializing Candidates");
	PCGEX_VALENCY_INFO(Solver, "States: %d, Modules to check: %d", ValencyStates->Num(), CompiledBondingRules->ModuleCount);

	int32 TotalCandidates = 0;
	int32 UnsolvableCount = 0;

	for (int32 StateIndex = 0; StateIndex < ValencyStates->Num(); ++StateIndex)
	{
		const PCGExValency::FValencyState& State = (*ValencyStates)[StateIndex];
		FWFCStateData& Data = StateData[StateIndex];

		// Skip already resolved (boundary) states
		if (State.IsResolved())
		{
			PCGEX_VALENCY_VERBOSE(Solver, "  State[%d]: ALREADY RESOLVED (ResolvedModule=%d)", StateIndex, State.ResolvedModule);
			continue;
		}

		Data.Candidates.Empty();

		// Get state orbital mask for logging
		const int64 StateMask = State.OrbitalMasks.Num() > 0 ? State.OrbitalMasks[0] : 0;

		// For each module, check if it fits this state
		for (int32 ModuleIndex = 0; ModuleIndex < CompiledBondingRules->ModuleCount; ++ModuleIndex)
		{
			if (DoesModuleFitState(ModuleIndex, State))
			{
				Data.Candidates.Add(ModuleIndex);
			}
		}

		TotalCandidates += Data.Candidates.Num();

		if (Data.Candidates.Num() == 0 && State.HasOrbitals())
		{
			PCGEX_VALENCY_WARNING(Solver, "  State[%d]: UNSOLVABLE - StateMask=0x%llX, no modules fit!", StateIndex, StateMask);
			(*ValencyStates)[StateIndex].ResolvedModule = PCGExValency::SlotState::UNSOLVABLE;
			UnsolvableCount++;
		}
		else
		{
			PCGEX_VALENCY_VERBOSE(Solver, "  State[%d]: StateMask=0x%llX, %d candidates: [%s]",
				StateIndex, StateMask, Data.Candidates.Num(),
				*FString::JoinBy(Data.Candidates, TEXT(", "), [](int32 Idx) { return FString::FromInt(Idx); }));
		}
	}

	VALENCY_LOG_SUBSECTION(Solver, "Candidates Init Complete");
	PCGEX_VALENCY_INFO(Solver, "Total candidates=%d, Unsolvable=%d", TotalCandidates, UnsolvableCount);
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

	VALENCY_LOG_SECTION(Solver, "ENTROPY SOLVER SOLVE START");

	if (!CompiledBondingRules || !ValencyStates)
	{
		PCGEX_VALENCY_ERROR(Solver, "Solve: Missing CompiledBondingRules or ValencyStates!");
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

	PCGEX_VALENCY_INFO(Solver, "Initial boundaries: %d, Queue size: %d", Result.BoundaryCount, EntropyQueue.Num());

	// Main solve loop
	int32 Iteration = 0;
	while (EntropyQueue.Num() > 0)
	{
		const int32 StateIndex = PopLowestEntropy();
		if (StateIndex < 0)
		{
			break;
		}

		PCGEX_VALENCY_VERBOSE(Solver, "--- Solve Iteration %d: Processing State[%d], Entropy=%.2f ---",
			Iteration, StateIndex, StateData[StateIndex].Entropy);

		if (!CollapseState(StateIndex))
		{
			PCGEX_VALENCY_WARNING(Solver, "  State[%d] CONTRADICTION - marked unsolvable", StateIndex);
			// Contradiction - state is now unsolvable but we continue with others
		}

		Iteration++;
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

	VALENCY_LOG_SECTION(Solver, "ENTROPY SOLVER SOLVE COMPLETE");
	PCGEX_VALENCY_INFO(Solver, "Iterations: %d, Resolved: %d, Unsolvable: %d, Boundaries: %d",
		Iteration, Result.ResolvedCount, Result.UnsolvableCount, Result.BoundaryCount);

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
		PCGEX_VALENCY_VERBOSE(Solver, "  CollapseState[%d]: Already resolved with module %d", StateIndex, State.ResolvedModule);
		return true;
	}

	PCGEX_VALENCY_VERBOSE(Solver, "  CollapseState[%d]: Candidates before filter: %d", StateIndex, Data.Candidates.Num());

	// Filter candidates based on current neighbor states
	if (!FilterCandidates(StateIndex))
	{
		PCGEX_VALENCY_WARNING(Solver, "  CollapseState[%d]: NO CANDIDATES after filter!", StateIndex);
		// No valid candidates - mark as unsolvable
		State.ResolvedModule = PCGExValency::SlotState::UNSOLVABLE;
		return false;
	}

	PCGEX_VALENCY_VERBOSE(Solver, "  CollapseState[%d]: Candidates after filter: %d [%s]",
		StateIndex, Data.Candidates.Num(),
		*FString::JoinBy(Data.Candidates, TEXT(", "), [](int32 Idx) { return FString::FromInt(Idx); }));

	// Select module using weighted random
	const int32 SelectedModule = SelectWeightedRandom(Data.Candidates);

	if (SelectedModule < 0)
	{
		PCGEX_VALENCY_WARNING(Solver, "  CollapseState[%d]: SelectWeightedRandom returned -1!", StateIndex);
		State.ResolvedModule = PCGExValency::SlotState::UNSOLVABLE;
		return false;
	}

	// Record the selection
	State.ResolvedModule = SelectedModule;
	Data.Candidates.Empty();
	DistributionTracker.RecordSpawn(SelectedModule, CompiledBondingRules);

	// Log the selection with asset info
	const FString AssetName = CompiledBondingRules->ModuleAssets.IsValidIndex(SelectedModule)
		? CompiledBondingRules->ModuleAssets[SelectedModule].GetAssetName()
		: TEXT("Unknown");
	PCGEX_VALENCY_VERBOSE(Solver, "  CollapseState[%d]: SELECTED Module[%d] = '%s'", StateIndex, SelectedModule, *AssetName);

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

	int32 RemovedByDistribution = 0;
	int32 RemovedByNeighbor = 0;

	// Filter out candidates that don't work with resolved neighbors
	for (int32 i = Data.Candidates.Num() - 1; i >= 0; --i)
	{
		const int32 CandidateModule = Data.Candidates[i];

		// Check distribution constraints
		if (!DistributionTracker.CanSpawn(CandidateModule))
		{
			PCGEX_VALENCY_VERBOSE(Solver, "    FilterCandidates: Module[%d] rejected by distribution constraints", CandidateModule);
			Data.Candidates.RemoveAt(i);
			RemovedByDistribution++;
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
				PCGEX_VALENCY_VERBOSE(Solver, "    FilterCandidates: Module[%d] incompatible with neighbor Module[%d] at orbital %d",
					CandidateModule, NeighborState.ResolvedModule, OrbitalIndex);
				bCompatible = false;
			}
		}

		if (!bCompatible)
		{
			Data.Candidates.RemoveAt(i);
			RemovedByNeighbor++;
		}
	}

	if (RemovedByDistribution > 0 || RemovedByNeighbor > 0)
	{
		PCGEX_VALENCY_VERBOSE(Solver, "    FilterCandidates[%d]: Removed %d by distribution, %d by neighbor constraints",
			StateIndex, RemovedByDistribution, RemovedByNeighbor);
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
