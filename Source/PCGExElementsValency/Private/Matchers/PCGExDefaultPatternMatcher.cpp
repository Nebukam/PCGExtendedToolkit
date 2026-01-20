// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Matchers/PCGExDefaultPatternMatcher.h"

#include "Core/PCGExValencyCommon.h"

TSharedPtr<FPCGExPatternMatcherOperation> UPCGExDefaultPatternMatcher::CreateOperation() const
{
	PCGEX_FACTORY_NEW_OPERATION(DefaultPatternMatcherOperation)

	// Initialize common settings from base factory
	InitOperation(NewOperation);

	// Set default matcher-specific settings
	NewOperation->OverlapResolution = OverlapResolution;

	return NewOperation;
}

PCGExPatternMatcher::FMatchResult FPCGExDefaultPatternMatcherOperation::Match()
{
	PCGExPatternMatcher::FMatchResult Result;

	if (!CompiledPatterns || !CompiledPatterns->HasPatterns())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher] No compiled patterns or empty patterns"));
		Result.bSuccess = true; // No patterns = nothing to match, but not an error
		return Result;
	}

	const FPCGExValencyPatternSetCompiled& PatternSet = *CompiledPatterns;

	UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher] NumNodes=%d, Patterns=%d (Exclusive=%d, Additive=%d)"),
		NumNodes, PatternSet.Patterns.Num(), PatternSet.ExclusivePatternIndices.Num(), PatternSet.AdditivePatternIndices.Num());

	// Log sample of node module indices
	if (NumNodes > 0)
	{
		FString SampleModules;
		const int32 SampleCount = FMath::Min(10, NumNodes);
		for (int32 i = 0; i < SampleCount; ++i)
		{
			SampleModules += FString::Printf(TEXT("%d,"), GetModuleIndex(i));
		}
		UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher] First %d node module indices: [%s]"), SampleCount, *SampleModules);

		// Count module occurrences to show distribution
		TMap<int32, int32> ModuleCounts;
		for (int32 i = 0; i < NumNodes; ++i)
		{
			const int32 ModuleIdx = GetModuleIndex(i);
			ModuleCounts.FindOrAdd(ModuleIdx)++;
		}
		UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher] Module distribution in solved cluster:"));
		for (const auto& Pair : ModuleCounts)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher]   Module[%d]: %d nodes"), Pair.Key, Pair.Value);
		}

		// Show adjacency info for nodes with module 2 (Box_solid) - what are their neighbors on ALL orbitals?
		// Also show positions and computed directions to prove spatial layout
		UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher] Adjacency detail for Module[2] nodes (all orbitals):"));
		UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher] NOTE: Orbital directions are East(0)=+X, West(1)=-X, North(2)=+Y, South(3)=-Y"));
		for (int32 i = 0; i < NumNodes; ++i)
		{
			if (GetModuleIndex(i) == 2)
			{
				const FVector NodePos = GetDebugNodePosition(i);
				UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher]   Node[%d] Position=%s OrbitalMask=0x%llX:"),
					i, *NodePos.ToString(), GetOrbitalMask(i));

				for (int32 Orbital = 0; Orbital < 4; ++Orbital)
				{
					const int32 NeighborNode = GetNeighborAtOrbital(i, Orbital);
					if (NeighborNode >= 0)
					{
						const int32 NeighborModule = GetModuleIndex(NeighborNode);
						const FVector NeighborPos = GetDebugNodePosition(NeighborNode);
						const FVector Direction = HasDebugPositions() ? (NeighborPos - NodePos).GetSafeNormal() : FVector::ZeroVector;

						// Interpret direction manually
						const TCHAR* DirName = TEXT("?");
						if (FMath::Abs(Direction.X) > 0.9f) { DirName = Direction.X > 0 ? TEXT("+X/East") : TEXT("-X/West"); }
						else if (FMath::Abs(Direction.Y) > 0.9f) { DirName = Direction.Y > 0 ? TEXT("+Y/North") : TEXT("-Y/South"); }
						else if (FMath::Abs(Direction.Z) > 0.9f) { DirName = Direction.Z > 0 ? TEXT("+Z") : TEXT("-Z"); }

						UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher]     Orbital[%d] → Neighbor Node[%d] Module[%d] Pos=%s Dir=%s (%s)"),
							Orbital, NeighborNode, NeighborModule, *NeighborPos.ToString(), *Direction.ToString(), DirName);
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher]     Orbital[%d] → NO NEIGHBOR"), Orbital);
					}
				}
			}
		}

		// Also log patterns' expected adjacency
		UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher] Pattern adjacency expectations:"));
		for (int32 PatternIdx = 0; PatternIdx < PatternSet.Patterns.Num(); ++PatternIdx)
		{
			const FPCGExValencyPatternCompiled& Pattern = PatternSet.Patterns[PatternIdx];
			UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher]   Pattern[%d] '%s':"), PatternIdx, *Pattern.Settings.PatternName.ToString());
			for (int32 EntryIdx = 0; EntryIdx < Pattern.Entries.Num(); ++EntryIdx)
			{
				const FPCGExValencyPatternEntryCompiled& Entry = Pattern.Entries[EntryIdx];
				FString ModulesStr;
				for (int32 M : Entry.ModuleIndices) { ModulesStr += FString::Printf(TEXT("%d,"), M); }
				UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher]     Entry[%d]: Modules=[%s] Adjacencies=%d"),
					EntryIdx, *ModulesStr, Entry.Adjacency.Num());
				for (const FIntVector& Adj : Entry.Adjacency)
				{
					const TCHAR* SrcOrbName = TEXT("?");
					if (Adj.Y == 0) SrcOrbName = TEXT("East/+X");
					else if (Adj.Y == 1) SrcOrbName = TEXT("West/-X");
					else if (Adj.Y == 2) SrcOrbName = TEXT("North/+Y");
					else if (Adj.Y == 3) SrcOrbName = TEXT("South/-Y");

					const TCHAR* TgtOrbName = TEXT("?");
					if (Adj.Z == 0) TgtOrbName = TEXT("East/+X");
					else if (Adj.Z == 1) TgtOrbName = TEXT("West/-X");
					else if (Adj.Z == 2) TgtOrbName = TEXT("North/+Y");
					else if (Adj.Z == 3) TgtOrbName = TEXT("South/-Y");

					UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher]       Entry[%d].Orbital[%d](%s) → Entry[%d].Orbital[%d](%s)"),
						EntryIdx, Adj.Y, SrcOrbName, Adj.X, Adj.Z, TgtOrbName);
				}
			}
		}
	}

	// Find matches for exclusive patterns first
	for (const int32 PatternIdx : PatternSet.ExclusivePatternIndices)
	{
		// Apply filter if set
		if (PatternFilter && !PatternFilter(PatternIdx, CompiledPatterns)) { continue; }

		const FPCGExValencyPatternCompiled& Pattern = PatternSet.Patterns[PatternIdx];
		UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher] Processing exclusive pattern %d: %s (entries=%d)"),
			PatternIdx, *Pattern.Settings.PatternName.ToString(), Pattern.Entries.Num());
		FindMatchesForPattern(PatternIdx, Pattern);
	}

	// Then find matches for additive patterns
	for (const int32 PatternIdx : PatternSet.AdditivePatternIndices)
	{
		// Apply filter if set
		if (PatternFilter && !PatternFilter(PatternIdx, CompiledPatterns)) { continue; }

		const FPCGExValencyPatternCompiled& Pattern = PatternSet.Patterns[PatternIdx];
		UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher] Processing additive pattern %d: %s (entries=%d)"),
			PatternIdx, *Pattern.Settings.PatternName.ToString(), Pattern.Entries.Num());
		FindMatchesForPattern(PatternIdx, Pattern);
	}

	// Resolve overlaps
	ResolveOverlaps();

	// Claim nodes for exclusive matches
	ClaimMatchedNodes();

	// Compute statistics
	TSet<int32> MatchedPatterns;
	TSet<int32> AnnotatedNodes;

	for (const FPCGExValencyPatternMatch& Match : Matches)
	{
		if (!Match.bClaimed && CompiledPatterns->Patterns[Match.PatternIndex].Settings.bExclusive)
		{
			continue; // Skip unclaimed exclusive matches
		}

		MatchedPatterns.Add(Match.PatternIndex);

		const FPCGExValencyPatternCompiled& Pattern = CompiledPatterns->Patterns[Match.PatternIndex];
		for (int32 EntryIdx = 0; EntryIdx < Match.EntryToNode.Num(); ++EntryIdx)
		{
			if (Pattern.Entries[EntryIdx].bIsActive)
			{
				AnnotatedNodes.Add(Match.EntryToNode[EntryIdx]);
			}
		}
	}

	Result.PatternsMatched = MatchedPatterns.Num();
	Result.NodesAnnotated = AnnotatedNodes.Num();
	Result.bSuccess = true;

	return Result;
}

void FPCGExDefaultPatternMatcherOperation::FindMatchesForPattern(int32 PatternIndex, const FPCGExValencyPatternCompiled& Pattern)
{
	if (!Pattern.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher] Pattern %d is invalid"), PatternIndex);
		return;
	}

	const FPCGExValencyPatternEntryCompiled& RootEntry = Pattern.Entries[0];

	UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher] Pattern %d root entry: Wildcard=%d, ModuleIndices=%d, BoundaryMask=%llu"),
		PatternIndex, RootEntry.bIsWildcard, RootEntry.ModuleIndices.Num(), RootEntry.BoundaryOrbitalMask);

	if (RootEntry.ModuleIndices.Num() > 0)
	{
		FString ModulesStr;
		for (int32 M : RootEntry.ModuleIndices) { ModulesStr += FString::Printf(TEXT("%d,"), M); }
		UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher] Root entry expects modules: [%s]"), *ModulesStr);
	}

	int32 SkippedClaimed = 0;
	int32 SkippedModuleMismatch = 0;
	int32 SkippedBoundary = 0;
	int32 TriedMatch = 0;

	// Try to match starting from each node that could match the root entry
	for (int32 NodeIdx = 0; NodeIdx < NumNodes; ++NodeIdx)
	{
		// Skip already claimed nodes for exclusive patterns
		if (Pattern.Settings.bExclusive && IsNodeClaimed(NodeIdx))
		{
			SkippedClaimed++;
			continue;
		}

		// Check if this node's module matches the root entry
		const int32 ModuleIndex = GetModuleIndex(NodeIdx);
		if (!RootEntry.MatchesModule(ModuleIndex))
		{
			SkippedModuleMismatch++;
			continue;
		}

		// Check boundary constraints for root entry
		// BoundaryOrbitalMask indicates orbitals that MUST be empty (no neighbor)
		if (RootEntry.BoundaryOrbitalMask != 0)
		{
			const int64 NodeOccupiedMask = GetOrbitalMask(NodeIdx);
			if ((NodeOccupiedMask & RootEntry.BoundaryOrbitalMask) != 0)
			{
				SkippedBoundary++;
				continue; // Boundary orbital has a neighbor, skip
			}
		}

		TriedMatch++;

		// Try to match the full pattern starting from this node
		FPCGExValencyPatternMatch Match;
		if (TryMatchPatternFromNode(PatternIndex, Pattern, NodeIdx, Match))
		{
			Matches.Add(MoveTemp(Match));
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher] Pattern %d: Nodes=%d, SkippedClaimed=%d, SkippedModuleMismatch=%d, SkippedBoundary=%d, TriedMatch=%d, TotalMatches=%d"),
		PatternIndex, NumNodes, SkippedClaimed, SkippedModuleMismatch, SkippedBoundary, TriedMatch, Matches.Num());
}

bool FPCGExDefaultPatternMatcherOperation::TryMatchPatternFromNode(
	int32 PatternIndex,
	const FPCGExValencyPatternCompiled& Pattern,
	int32 StartNodeIndex,
	FPCGExValencyPatternMatch& OutMatch)
{
	const int32 NumEntries = Pattern.GetEntryCount();

	UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher] TryMatch: Pattern %d, StartNode %d (module=%d), NumEntries=%d"),
		PatternIndex, StartNodeIndex, GetModuleIndex(StartNodeIndex), NumEntries);

	// Initialize mapping
	TArray<int32> EntryToNode;
	EntryToNode.SetNum(NumEntries);
	for (int32 i = 0; i < NumEntries; ++i)
	{
		EntryToNode[i] = -1;
	}

	// Start with root entry mapped to start node
	EntryToNode[0] = StartNodeIndex;

	TSet<int32> UsedNodes;
	UsedNodes.Add(StartNodeIndex);

	// DFS to match remaining entries
	if (!MatchEntryRecursive(Pattern, 0, EntryToNode, UsedNodes))
	{
		UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher] TryMatch FAILED for node %d"), StartNodeIndex);
		return false;
	}

	// Verify all entries were matched
	for (int32 i = 0; i < NumEntries; ++i)
	{
		if (EntryToNode[i] < 0)
		{
			return false;
		}
	}

	// Build the match result
	OutMatch.PatternIndex = PatternIndex;
	OutMatch.EntryToNode = MoveTemp(EntryToNode);
	OutMatch.bClaimed = false;

	return true;
}

bool FPCGExDefaultPatternMatcherOperation::MatchEntryRecursive(
	const FPCGExValencyPatternCompiled& Pattern,
	int32 EntryIndex,
	TArray<int32>& EntryToNode,
	TSet<int32>& UsedNodes)
{
	const FPCGExValencyPatternEntryCompiled& Entry = Pattern.Entries[EntryIndex];
	const int32 CurrentNode = EntryToNode[EntryIndex];

	const FVector CurrentPos = GetDebugNodePosition(CurrentNode);
	UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher] MatchRecursive: EntryIdx=%d, CurrentNode=%d (pos=%s), Adjacencies=%d"),
		EntryIndex, CurrentNode, *CurrentPos.ToString(), Entry.Adjacency.Num());

	// Process all adjacencies from this entry
	for (const FIntVector& Adj : Entry.Adjacency)
	{
		const int32 TargetEntryIdx = Adj.X;
		const int32 SourceOrbital = Adj.Y;
		const int32 TargetOrbital = Adj.Z;

		// Map orbital to direction name for logging
		auto GetOrbitalName = [](int32 Orbital) -> const TCHAR*
		{
			switch (Orbital)
			{
			case 0: return TEXT("East/+X");
			case 1: return TEXT("West/-X");
			case 2: return TEXT("North/+Y");
			case 3: return TEXT("South/-Y");
			default: return TEXT("Unknown");
			}
		};

		UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher]   Adjacency: TargetEntry=%d, SrcOrbital=%d (%s), TgtOrbital=%d (%s)"),
			TargetEntryIdx, SourceOrbital, GetOrbitalName(SourceOrbital), TargetOrbital, GetOrbitalName(TargetOrbital));

		// Check if target entry is already matched
		if (EntryToNode[TargetEntryIdx] >= 0)
		{
			// Verify the existing match is correct
			const int32 ExistingNode = EntryToNode[TargetEntryIdx];
			const int32 NeighborAtOrbital = GetNeighborAtOrbital(CurrentNode, SourceOrbital);
			if (NeighborAtOrbital != ExistingNode)
			{
				UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher]   FAIL: Inconsistent match (existing=%d, neighbor=%d)"), ExistingNode, NeighborAtOrbital);
				return false; // Inconsistent match
			}
			continue;
		}

		// Find the neighbor at the specified orbital
		const int32 NeighborNode = GetNeighborAtOrbital(CurrentNode, SourceOrbital);
		UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher]   NeighborAtOrbital(%d, %d) = %d"), CurrentNode, SourceOrbital, NeighborNode);

		if (NeighborNode < 0)
		{
			// Log all available neighbors with positions and directions to show what IS available
			UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher]   FAIL: No neighbor at orbital %d (%s). Available neighbors:"),
				SourceOrbital, GetOrbitalName(SourceOrbital));
			for (int32 O = 0; O < 4; ++O)
			{
				const int32 N = GetNeighborAtOrbital(CurrentNode, O);
				if (N >= 0)
				{
					const FVector NPos = GetDebugNodePosition(N);
					const FVector Dir = HasDebugPositions() ? (NPos - CurrentPos).GetSafeNormal() : FVector::ZeroVector;
					const int32 NMod = GetModuleIndex(N);
					UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher]     Orbital[%d](%s) → Node[%d] Module[%d] Pos=%s Dir=%s"),
						O, GetOrbitalName(O), N, NMod, *NPos.ToString(), *Dir.ToString());
				}
			}
			UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher]   Pattern expected neighbor on orbital %d (%s), but that orbital is empty."),
				SourceOrbital, GetOrbitalName(SourceOrbital));
			return false; // No neighbor at required orbital
		}

		// Log actual direction to neighbor for debugging
		const FVector NeighborPos = GetDebugNodePosition(NeighborNode);
		const FVector ActualDir = HasDebugPositions() ? (NeighborPos - CurrentPos).GetSafeNormal() : FVector::ZeroVector;
		UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher]   Found neighbor: Node[%d] pos=%s, ActualDir=%s"),
			NeighborNode, *NeighborPos.ToString(), *ActualDir.ToString());

		// Check if this node is already used by another entry
		if (UsedNodes.Contains(NeighborNode))
		{
			UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher]   FAIL: Node %d already used"), NeighborNode);
			return false; // Node already used (patterns shouldn't have cycles to same node)
		}

		// Check if the neighbor's module matches the target entry
		const FPCGExValencyPatternEntryCompiled& TargetEntry = Pattern.Entries[TargetEntryIdx];
		const int32 NeighborModule = GetModuleIndex(NeighborNode);

		FString ExpectedModulesStr;
		for (int32 M : TargetEntry.ModuleIndices) { ExpectedModulesStr += FString::Printf(TEXT("%d,"), M); }
		UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher]   NeighborModule=%d, TargetEntry expects wildcard=%d, modules=[%s]"),
			NeighborModule, TargetEntry.bIsWildcard, *ExpectedModulesStr);

		if (!TargetEntry.MatchesModule(NeighborModule))
		{
			UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher]   FAIL: Module mismatch (neighbor has %d, expected [%s])"), NeighborModule, *ExpectedModulesStr);
			return false;
		}

		// Check boundary constraints for target entry
		if (TargetEntry.BoundaryOrbitalMask != 0)
		{
			const int64 NeighborOccupiedMask = GetOrbitalMask(NeighborNode);
			if ((NeighborOccupiedMask & TargetEntry.BoundaryOrbitalMask) != 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher]   FAIL: Boundary constraint (mask=%llu, occupied=%llu)"),
					TargetEntry.BoundaryOrbitalMask, NeighborOccupiedMask);
				return false; // Boundary orbital has a neighbor
			}
		}

		// Verify the reverse connection (neighbor connects back via expected orbital)
		const int32 ReverseNeighbor = GetNeighborAtOrbital(NeighborNode, TargetOrbital);
		UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher]   ReverseCheck: GetNeighborAtOrbital(%d, %d) = %d (expecting %d)"),
			NeighborNode, TargetOrbital, ReverseNeighbor, CurrentNode);

		if (ReverseNeighbor != CurrentNode)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher]   FAIL: Reverse connection mismatch"));
			return false; // Orbital connection is not bidirectional as expected
		}

		// Match found - assign and recurse
		UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher]   SUCCESS: Matched entry %d to node %d"), TargetEntryIdx, NeighborNode);
		EntryToNode[TargetEntryIdx] = NeighborNode;
		UsedNodes.Add(NeighborNode);

		// Recursively match from the new entry
		if (!MatchEntryRecursive(Pattern, TargetEntryIdx, EntryToNode, UsedNodes))
		{
			// Backtrack
			UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher]   Backtracking from entry %d"), TargetEntryIdx);
			EntryToNode[TargetEntryIdx] = -1;
			UsedNodes.Remove(NeighborNode);
			return false;
		}
	}

	return true;
}

void FPCGExDefaultPatternMatcherOperation::ResolveOverlaps()
{
	if (Matches.IsEmpty()) { return; }

	// Sort matches based on resolution strategy
	switch (OverlapResolution)
	{
	case EPCGExPatternOverlapResolution::WeightBased:
		Matches.Sort([this](const FPCGExValencyPatternMatch& A, const FPCGExValencyPatternMatch& B)
		{
			const float WeightA = CompiledPatterns->Patterns[A.PatternIndex].Settings.Weight;
			const float WeightB = CompiledPatterns->Patterns[B.PatternIndex].Settings.Weight;
			return WeightA > WeightB;
		});
		break;

	case EPCGExPatternOverlapResolution::LargestFirst:
		Matches.Sort([](const FPCGExValencyPatternMatch& A, const FPCGExValencyPatternMatch& B)
		{
			return A.EntryToNode.Num() > B.EntryToNode.Num();
		});
		break;

	case EPCGExPatternOverlapResolution::SmallestFirst:
		Matches.Sort([](const FPCGExValencyPatternMatch& A, const FPCGExValencyPatternMatch& B)
		{
			return A.EntryToNode.Num() < B.EntryToNode.Num();
		});
		break;

	case EPCGExPatternOverlapResolution::FirstDefined:
		// Keep original order
		break;
	}
}

void FPCGExDefaultPatternMatcherOperation::ClaimMatchedNodes()
{
	UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher] ClaimMatchedNodes: bExclusive=%d, Matches=%d"), bExclusive, Matches.Num());

	if (!bExclusive)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher] ClaimMatchedNodes: Skipping (bExclusive=false)"));
		return;
	}

	// Claim nodes for exclusive patterns (in sorted order)
	for (FPCGExValencyPatternMatch& Match : Matches)
	{
		const FPCGExValencyPatternCompiled& Pattern = CompiledPatterns->Patterns[Match.PatternIndex];

		UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher] ClaimMatchedNodes: Match PatternIdx=%d, Pattern.bExclusive=%d"),
			Match.PatternIndex, Pattern.Settings.bExclusive);

		if (!Pattern.Settings.bExclusive)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher] ClaimMatchedNodes: Skipping non-exclusive pattern"));
			continue;
		}

		// Check if any active nodes are already claimed
		bool bCanClaim = true;
		for (int32 EntryIdx = 0; EntryIdx < Pattern.Entries.Num(); ++EntryIdx)
		{
			if (!Pattern.Entries[EntryIdx].bIsActive) { continue; }

			const int32 NodeIdx = Match.EntryToNode[EntryIdx];
			if (IsNodeClaimed(NodeIdx))
			{
				UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher] ClaimMatchedNodes: Node %d already claimed"), NodeIdx);
				bCanClaim = false;
				break;
			}
		}

		if (bCanClaim)
		{
			Match.bClaimed = true;
			UE_LOG(LogTemp, Warning, TEXT("[DefaultMatcher] ClaimMatchedNodes: Claimed match for pattern %d"), Match.PatternIndex);

			// Claim all active nodes
			for (int32 EntryIdx = 0; EntryIdx < Pattern.Entries.Num(); ++EntryIdx)
			{
				if (!Pattern.Entries[EntryIdx].bIsActive) { continue; }
				ClaimNode(Match.EntryToNode[EntryIdx]);
			}
		}
	}
}
