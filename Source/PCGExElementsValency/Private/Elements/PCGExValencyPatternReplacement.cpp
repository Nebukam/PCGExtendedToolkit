// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExValencyPatternReplacement.h"

#include "Clusters/PCGExCluster.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Data/Utils/PCGExDataPreloader.h"

#define LOCTEXT_NAMESPACE "PCGExValencyPatternReplacement"
#define PCGEX_NAMESPACE ValencyPatternReplacement

TArray<FPCGPinProperties> UPCGExValencyPatternReplacementSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExValencyPatternReplacementSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();

	if (bOutputMatchedPoints)
	{
		PCGEX_PIN_POINTS(TEXT("Matched"), "Points that were matched by patterns (for Remove/Fork strategies)", Required)
	}

	return PinProperties;
}

FPCGElementPtr UPCGExValencyPatternReplacementSettings::CreateElement() const
{
	return MakeShared<FPCGExValencyPatternReplacementElement>();
}

PCGExData::EIOInit UPCGExValencyPatternReplacementSettings::GetMainOutputInitMode() const
{
	return PCGExData::EIOInit::Duplicate;
}

PCGExData::EIOInit UPCGExValencyPatternReplacementSettings::GetEdgeOutputInitMode() const
{
	return PCGExData::EIOInit::Forward;
}

PCGEX_ELEMENT_BATCH_EDGE_IMPL_ADV(ValencyPatternReplacement)

bool FPCGExValencyPatternReplacementElement::PostBoot(FPCGExContext* InContext) const
{
	// Base class handles BondingRules and OrbitalSet validation
	if (!FPCGExValencyProcessorElement::PostBoot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ValencyPatternReplacement)

	// Get compiled data
	if (!Context->BondingRules->IsCompiled())
	{
		PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("Bonding Rules has no compiled data. Please rebuild."));
		return false;
	}

	const TSharedPtr<FPCGExValencyBondingRulesCompiled>& CompiledData = Context->BondingRules->CompiledData;
	if (!CompiledData)
	{
		PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("Bonding Rules has no compiled data. Please rebuild."));
		return false;
	}

	// Check for patterns
	if (!CompiledData->CompiledPatterns.HasPatterns())
	{
		if (!Settings->bQuietNoPatterns)
		{
			PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("Bonding Rules has no patterns. Pattern replacement will have no effect."));
		}
		// Still return true - no patterns means pass-through
	}

	Context->CompiledPatterns = &CompiledData->CompiledPatterns;

	return true;
}

bool FPCGExValencyPatternReplacementElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	PCGEX_CONTEXT_AND_SETTINGS(ValencyPatternReplacement)

	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}

namespace PCGExValencyPatternReplacement
{
	FBatch::FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
		: TBatch(InContext, InVtx, InEdges)
	{
	}

	FBatch::~FBatch()
	{
	}

	void FBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TBatch::RegisterBuffersDependencies(FacadePreloader);

		// Register module index reader (attribute name from OrbitalSet)
		const FPCGExValencyPatternReplacementContext* Context = GetContext<FPCGExValencyPatternReplacementContext>();
		if (Context && Context->OrbitalSet)
		{
			FacadePreloader.Register<int32>(ExecutionContext, Context->OrbitalSet->GetModuleIdxAttributeName());
		}
	}

	void FBatch::OnProcessingPreparationComplete()
	{
		const UPCGExValencyPatternReplacementSettings* Settings = ExecutionContext->GetInputSettings<UPCGExValencyPatternReplacementSettings>();
		const FPCGExValencyPatternReplacementContext* Context = GetContext<FPCGExValencyPatternReplacementContext>();

		// Create module index reader and writer (attribute name from OrbitalSet)
		if (Context && Context->OrbitalSet)
		{
			const FName ModuleIdxAttrName = Context->OrbitalSet->GetModuleIdxAttributeName();
			ModuleIndexReader = VtxDataFacade->GetReadable<int32>(ModuleIdxAttrName);
			// Writer uses inherit mode since we're modifying existing Staging output
			ModuleIndexWriter = VtxDataFacade->GetWritable<int32>(ModuleIdxAttrName, -1, true, PCGExData::EBufferInit::Inherit);
		}

		// Create pattern name writer
		PatternNameWriter = VtxDataFacade->GetWritable<FName>(Settings->PatternNameAttributeName, FName(NAME_None), true, PCGExData::EBufferInit::New);

		// Create pattern match index writer
		PatternMatchIndexWriter = VtxDataFacade->GetWritable<int32>(Settings->PatternMatchIndexAttributeName, -1, true, PCGExData::EBufferInit::New);

		TBatch::OnProcessingPreparationComplete();
	}

	bool FBatch::PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor)
	{
		if (!TBatch::PrepareSingle(InProcessor)) { return false; }

		FProcessor* Processor = static_cast<FProcessor*>(InProcessor.Get());
		Processor->ModuleIndexReader = ModuleIndexReader;
		Processor->ModuleIndexWriter = ModuleIndexWriter;
		Processor->PatternNameWriter = PatternNameWriter;
		Processor->PatternMatchIndexWriter = PatternMatchIndexWriter;

		return true;
	}

	void FBatch::Write()
	{
		TBatch::Write();
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		// Parent handles: edge indices reader, BuildOrbitalCache(), InitializeValencyStates()
		if (!TProcessor::Process(InTaskManager)) { return false; }

		// Validate module index reader
		if (!ModuleIndexReader)
		{
			return false;
		}

		return true;
	}

	void FProcessor::ProcessNodes(const PCGExMT::FScope& Scope)
	{
		// Pattern matching is done in OnNodesProcessingComplete since it needs all nodes
	}

	void FProcessor::OnNodesProcessingComplete()
	{
		TProcessor::OnNodesProcessingComplete();

		if (!Context->CompiledPatterns || !Context->CompiledPatterns->HasPatterns())
		{
			return;
		}

		const FPCGExValencyPatternSetCompiled& PatternSet = *Context->CompiledPatterns;

		// Find matches for all exclusive patterns first
		for (int32 PatternIdx : PatternSet.ExclusivePatternIndices)
		{
			const FPCGExValencyPatternCompiled& Pattern = PatternSet.Patterns[PatternIdx];
			FindMatchesForPattern(PatternIdx, Pattern);
		}

		// Then find matches for additive patterns
		for (int32 PatternIdx : PatternSet.AdditivePatternIndices)
		{
			const FPCGExValencyPatternCompiled& Pattern = PatternSet.Patterns[PatternIdx];
			FindMatchesForPattern(PatternIdx, Pattern);
		}

		// Resolve overlaps
		ResolveOverlaps();

		// Apply matches
		ApplyMatches();
	}

	void FProcessor::Write()
	{
		TProcessor::Write();

		// Apply collapse replacement transforms
		if (!CollapseReplacements.IsEmpty())
		{
			TPCGValueRange<FTransform> OutTransforms = VtxDataFacade->GetOut()->GetTransformValueRange();
			for (const auto& Pair : CollapseReplacements)
			{
				OutTransforms[Pair.Key] = Pair.Value;
			}
		}

		// Apply swap module indices
		if (!SwapTargets.IsEmpty() && ModuleIndexWriter)
		{
			for (const auto& Pair : SwapTargets)
			{
				ModuleIndexWriter->SetValue(Pair.Key, Pair.Value);
			}
		}

		// Mark nodes for removal (actual removal happens in batch Write via point filtering)
		// The NodesToRemove set is used by the batch to filter points
	}

	void FProcessor::FindMatchesForPattern(int32 PatternIndex, const FPCGExValencyPatternCompiled& Pattern)
	{
		if (!Pattern.IsValid()) { return; }

		const FPCGExValencyPatternEntryCompiled& RootEntry = Pattern.Entries[0];
		const int32 NodeCount = OrbitalCache->GetNumNodes();

		// Try to match starting from each node that could match the root entry
		for (int32 NodeIdx = 0; NodeIdx < NodeCount; ++NodeIdx)
		{
			// Skip already claimed nodes for exclusive patterns
			if (Pattern.Settings.bExclusive && ClaimedNodes.Contains(NodeIdx))
			{
				continue;
			}

			// Check if this node's module matches the root entry
			const int32 ModuleIndex = ModuleIndexReader->Read(NodeIdx);
			if (!RootEntry.MatchesModule(ModuleIndex))
			{
				continue;
			}

			// Check boundary constraints for root entry
			// BoundaryOrbitalMask indicates orbitals that MUST be empty (no neighbor)
			// OrbitalCache->GetOrbitalMask returns which orbitals have neighbors
			if (RootEntry.BoundaryOrbitalMask != 0)
			{
				const int64 NodeOccupiedMask = OrbitalCache->GetOrbitalMask(NodeIdx);
				if ((NodeOccupiedMask & RootEntry.BoundaryOrbitalMask) != 0)
				{
					continue; // Boundary orbital has a neighbor, skip
				}
			}

			// Try to match the full pattern starting from this node
			FPCGExValencyPatternMatch Match;
			if (TryMatchPatternFromNode(PatternIndex, Pattern, NodeIdx, Match))
			{
				AllMatches.Add(MoveTemp(Match));
			}
		}
	}

	bool FProcessor::TryMatchPatternFromNode(int32 PatternIndex, const FPCGExValencyPatternCompiled& Pattern, int32 StartNodeIndex, FPCGExValencyPatternMatch& OutMatch)
	{
		const int32 NumEntries = Pattern.GetEntryCount();

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
		OutMatch.ReplacementTransform = ComputeReplacementTransform(OutMatch, Pattern);
		OutMatch.bClaimed = false;

		return true;
	}

	bool FProcessor::MatchEntryRecursive(
		const FPCGExValencyPatternCompiled& Pattern,
		int32 EntryIndex,
		TArray<int32>& EntryToNode,
		TSet<int32>& UsedNodes)
	{
		const FPCGExValencyPatternEntryCompiled& Entry = Pattern.Entries[EntryIndex];
		const int32 CurrentNode = EntryToNode[EntryIndex];

		// Process all adjacencies from this entry
		for (const FIntVector& Adj : Entry.Adjacency)
		{
			const int32 TargetEntryIdx = Adj.X;
			const int32 SourceOrbital = Adj.Y;
			const int32 TargetOrbital = Adj.Z;

			// Check if target entry is already matched
			if (EntryToNode[TargetEntryIdx] >= 0)
			{
				// Verify the existing match is correct
				const int32 ExistingNode = EntryToNode[TargetEntryIdx];
				const int32 NeighborAtOrbital = OrbitalCache->GetNeighborAtOrbital(CurrentNode, SourceOrbital);
				if (NeighborAtOrbital != ExistingNode)
				{
					return false; // Inconsistent match
				}
				continue;
			}

			// Find the neighbor at the specified orbital
			const int32 NeighborNode = OrbitalCache->GetNeighborAtOrbital(CurrentNode, SourceOrbital);
			if (NeighborNode < 0)
			{
				return false; // No neighbor at required orbital
			}

			// Check if this node is already used by another entry
			if (UsedNodes.Contains(NeighborNode))
			{
				// Could be a valid cycle - check if it's the expected node
				// For now, treat as invalid (patterns shouldn't have cycles to same node)
				return false;
			}

			// Check if the neighbor's module matches the target entry
			const FPCGExValencyPatternEntryCompiled& TargetEntry = Pattern.Entries[TargetEntryIdx];
			const int32 NeighborModule = ModuleIndexReader->Read(NeighborNode);
			if (!TargetEntry.MatchesModule(NeighborModule))
			{
				return false;
			}

			// Check boundary constraints for target entry
			if (TargetEntry.BoundaryOrbitalMask != 0)
			{
				const int64 NeighborOccupiedMask = OrbitalCache->GetOrbitalMask(NeighborNode);
				if ((NeighborOccupiedMask & TargetEntry.BoundaryOrbitalMask) != 0)
				{
					return false; // Boundary orbital has a neighbor
				}
			}

			// Verify the reverse connection (neighbor connects back via expected orbital)
			const int32 ReverseNeighbor = OrbitalCache->GetNeighborAtOrbital(NeighborNode, TargetOrbital);
			if (ReverseNeighbor != CurrentNode)
			{
				return false; // Orbital connection is not bidirectional as expected
			}

			// Match found - assign and recurse
			EntryToNode[TargetEntryIdx] = NeighborNode;
			UsedNodes.Add(NeighborNode);

			// Recursively match from the new entry
			if (!MatchEntryRecursive(Pattern, TargetEntryIdx, EntryToNode, UsedNodes))
			{
				// Backtrack
				EntryToNode[TargetEntryIdx] = -1;
				UsedNodes.Remove(NeighborNode);
				return false;
			}
		}

		return true;
	}

	void FProcessor::ResolveOverlaps()
	{
		if (AllMatches.IsEmpty()) { return; }

		// Sort matches based on resolution strategy
		switch (Settings->OverlapResolution)
		{
		case EPCGExPatternOverlapResolution::WeightBased:
			// Sort by weight (highest first)
			AllMatches.Sort([this](const FPCGExValencyPatternMatch& A, const FPCGExValencyPatternMatch& B)
			{
				const float WeightA = Context->CompiledPatterns->Patterns[A.PatternIndex].Settings.Weight;
				const float WeightB = Context->CompiledPatterns->Patterns[B.PatternIndex].Settings.Weight;
				return WeightA > WeightB;
			});
			break;

		case EPCGExPatternOverlapResolution::LargestFirst:
			AllMatches.Sort([](const FPCGExValencyPatternMatch& A, const FPCGExValencyPatternMatch& B)
			{
				return A.EntryToNode.Num() > B.EntryToNode.Num();
			});
			break;

		case EPCGExPatternOverlapResolution::SmallestFirst:
			AllMatches.Sort([](const FPCGExValencyPatternMatch& A, const FPCGExValencyPatternMatch& B)
			{
				return A.EntryToNode.Num() < B.EntryToNode.Num();
			});
			break;

		case EPCGExPatternOverlapResolution::FirstDefined:
			// Keep original order
			break;
		}

		// Claim nodes for exclusive patterns (in sorted order)
		for (FPCGExValencyPatternMatch& Match : AllMatches)
		{
			const FPCGExValencyPatternCompiled& Pattern = Context->CompiledPatterns->Patterns[Match.PatternIndex];

			if (!Pattern.Settings.bExclusive)
			{
				continue;
			}

			// Check if any active nodes are already claimed
			bool bCanClaim = true;
			for (int32 EntryIdx = 0; EntryIdx < Pattern.Entries.Num(); ++EntryIdx)
			{
				if (!Pattern.Entries[EntryIdx].bIsActive) { continue; }

				const int32 NodeIdx = Match.EntryToNode[EntryIdx];
				if (ClaimedNodes.Contains(NodeIdx))
				{
					bCanClaim = false;
					break;
				}
			}

			if (bCanClaim)
			{
				Match.bClaimed = true;

				// Claim all active nodes
				for (int32 EntryIdx = 0; EntryIdx < Pattern.Entries.Num(); ++EntryIdx)
				{
					if (!Pattern.Entries[EntryIdx].bIsActive) { continue; }
					ClaimedNodes.Add(Match.EntryToNode[EntryIdx]);
				}
			}
		}
	}

	void FProcessor::ApplyMatches()
	{
		if (AllMatches.IsEmpty()) { return; }

		int32 MatchCounter = 0;

		for (const FPCGExValencyPatternMatch& Match : AllMatches)
		{
			if (!Match.bClaimed && Context->CompiledPatterns->Patterns[Match.PatternIndex].Settings.bExclusive)
			{
				continue; // Skip unclaimed exclusive matches
			}

			const FPCGExValencyPatternCompiled& Pattern = Context->CompiledPatterns->Patterns[Match.PatternIndex];
			const EPCGExPatternOutputStrategy Strategy = Pattern.Settings.OutputStrategy;

			// Annotate matched nodes (all strategies get annotation)
			for (int32 EntryIdx = 0; EntryIdx < Match.EntryToNode.Num(); ++EntryIdx)
			{
				const FPCGExValencyPatternEntryCompiled& Entry = Pattern.Entries[EntryIdx];
				if (!Entry.bIsActive) { continue; }

				const int32 NodeIdx = Match.EntryToNode[EntryIdx];

				// Write pattern name
				if (PatternNameWriter)
				{
					PatternNameWriter->SetValue(NodeIdx, Pattern.Settings.PatternName);
				}

				// Write match index
				if (PatternMatchIndexWriter)
				{
					PatternMatchIndexWriter->SetValue(NodeIdx, MatchCounter);
				}
			}

			// Apply output strategy
			switch (Strategy)
			{
			case EPCGExPatternOutputStrategy::Remove:
			case EPCGExPatternOutputStrategy::Fork:
				// Mark active nodes for removal/forking
				for (int32 EntryIdx = 0; EntryIdx < Match.EntryToNode.Num(); ++EntryIdx)
				{
					if (Pattern.Entries[EntryIdx].bIsActive)
					{
						NodesToRemove.Add(Match.EntryToNode[EntryIdx]);
					}
				}
				break;

			case EPCGExPatternOutputStrategy::Collapse:
				// Mark all active nodes for removal except one (which gets the replacement transform)
				{
					bool bFirstActive = true;
					for (int32 EntryIdx = 0; EntryIdx < Match.EntryToNode.Num(); ++EntryIdx)
					{
						if (!Pattern.Entries[EntryIdx].bIsActive) { continue; }

						const int32 NodeIdx = Match.EntryToNode[EntryIdx];
						if (bFirstActive)
						{
							// First active node becomes the collapsed point
							CollapseReplacements.Add(NodeIdx, Match.ReplacementTransform);
							bFirstActive = false;
						}
						else
						{
							// Other active nodes are removed
							NodesToRemove.Add(NodeIdx);
						}
					}
				}
				break;

			case EPCGExPatternOutputStrategy::Swap:
				// Update module index to swap target
				if (Pattern.SwapTargetModuleIndex >= 0)
				{
					for (int32 EntryIdx = 0; EntryIdx < Match.EntryToNode.Num(); ++EntryIdx)
					{
						if (Pattern.Entries[EntryIdx].bIsActive)
						{
							SwapTargets.Add(Match.EntryToNode[EntryIdx], Pattern.SwapTargetModuleIndex);
						}
					}
				}
				break;

			case EPCGExPatternOutputStrategy::Annotate:
				// Already done above, nothing more to do
				break;
			}

			++MatchCounter;
		}
	}

	FTransform FProcessor::ComputeReplacementTransform(const FPCGExValencyPatternMatch& Match, const FPCGExValencyPatternCompiled& Pattern)
	{
		TConstPCGValueRange<FTransform> Transforms = VtxDataFacade->Source->GetIn()->GetConstTransformValueRange();

		switch (Pattern.Settings.TransformMode)
		{
		case EPCGExPatternTransformMode::Centroid:
			{
				FVector Centroid = FVector::ZeroVector;
				int32 ActiveCount = 0;

				for (int32 EntryIdx = 0; EntryIdx < Pattern.Entries.Num(); ++EntryIdx)
				{
					if (!Pattern.Entries[EntryIdx].bIsActive) { continue; }

					const int32 NodeIdx = Match.EntryToNode[EntryIdx];
					Centroid += Transforms[NodeIdx].GetLocation();
					++ActiveCount;
				}

				if (ActiveCount > 0)
				{
					Centroid /= ActiveCount;
				}

				return FTransform(Centroid);
			}

		case EPCGExPatternTransformMode::PatternRoot:
			{
				// Use root entry's node transform
				const int32 RootNodeIdx = Match.EntryToNode[0];
				return Transforms[RootNodeIdx];
			}

		case EPCGExPatternTransformMode::FirstMatch:
			{
				// Use first active entry's node transform
				for (int32 EntryIdx = 0; EntryIdx < Pattern.Entries.Num(); ++EntryIdx)
				{
					if (Pattern.Entries[EntryIdx].bIsActive)
					{
						const int32 NodeIdx = Match.EntryToNode[EntryIdx];
						return Transforms[NodeIdx];
					}
				}
				return FTransform::Identity;
			}
		}

		return FTransform::Identity;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
