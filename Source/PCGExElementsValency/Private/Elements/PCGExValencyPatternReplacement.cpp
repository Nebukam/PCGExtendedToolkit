// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExValencyPatternReplacement.h"

#include "Clusters/PCGExCluster.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Data/Utils/PCGExDataPreloader.h"
#include "Matchers/PCGExDefaultPatternMatcher.h"

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

bool FPCGExValencyPatternReplacementElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExValencyProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ValencyPatternReplacement)

	PCGEX_OPERATION_VALIDATE(Matcher)

	return true;
}

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

	// Check for patterns - Pattern Replacement requires patterns to function
	if (!CompiledData->CompiledPatterns.HasPatterns())
	{
		if (!Settings->bQuietNoPatterns)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Bonding Rules has no patterns. Pattern Replacement requires patterns to be defined."));
		}
		return false;
	}

	Context->CompiledPatterns = &CompiledData->CompiledPatterns;

	// Register matcher factory from Settings
	if (Settings->Matcher)
	{
		Context->MatcherFactory = PCGEX_OPERATION_REGISTER_C(Context, UPCGExPatternMatcherFactory, Settings->Matcher, NAME_None);
	}

	if (!Context->MatcherFactory && !Settings->bQuietNoMatcher)
	{
		PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("No pattern matcher configured. Node will only annotate patterns using default subgraph matching."));
	}

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
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		// Parent handles: edge indices reader, BuildOrbitalCache(), InitializeValencyStates()
		if (!TProcessor::Process(InTaskManager)) { return false; }

		// Validate module data reader (required for pattern matching)
		if (!ModuleDataReader)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Module data attribute not found. Run Valency Staging with patterns first."));
			return false;
		}

		if (!Context->CompiledPatterns || !Context->CompiledPatterns->HasPatterns())
		{
			return true; // No patterns = nothing to match, but not an error
		}

		if (!OrbitalCache)
		{
			return false;
		}

		// Run pattern matching
		RunMatching();

		Write();

		return true;
	}

	void FProcessor::RunMatching()
	{
		const int32 NodesCount = OrbitalCache->GetNumNodes();
		const int32 Seed = VtxDataFacade->Source->IOIndex;

		// Create matcher operation from factory (or use default if none provided)
		if (Context->MatcherFactory)
		{
			MatcherOperation = Context->MatcherFactory->CreateOperation();
		}

		// If no factory provided or factory failed, create a default subgraph matcher
		if (!MatcherOperation)
		{
			TSharedPtr<FPCGExDefaultPatternMatcherOperation> DefaultOp = MakeShared<FPCGExDefaultPatternMatcherOperation>();
			DefaultOp->bExclusive = true;
			DefaultOp->OverlapResolution = EPCGExPatternOverlapResolution::WeightBased;
			MatcherOperation = DefaultOp;
		}

		// Initialize the operation with shared state
		MatcherOperation->Initialize(
			Cluster,
			Context->CompiledPatterns,
			OrbitalCache.Get(),
			ModuleDataReader,
			NodesCount,
			&ClaimedNodes,
			Seed, MatcherAllocations);

		// Set NodeIndex to PointIndex mapping (REQUIRED for buffer access)
		// Also set debug node positions for detailed logging
		{
			TConstPCGValueRange<FTransform> Transforms = VtxDataFacade->GetIn()->GetConstTransformValueRange();
			TArray<int32> NodeToPointMapping;
			TArray<FVector> NodePositions;
			NodeToPointMapping.SetNum(NodesCount);
			NodePositions.SetNum(NodesCount);
			const TArray<PCGExClusters::FNode>& Nodes = *Cluster->Nodes;
			for (int32 i = 0; i < NodesCount; ++i)
			{
				NodeToPointMapping[i] = Nodes[i].PointIndex;
				NodePositions[i] = Transforms[Nodes[i].PointIndex].GetLocation();
			}
			MatcherOperation->SetNodeToPointMapping(NodeToPointMapping);
			MatcherOperation->SetDebugNodePositions(NodePositions);
		}

		// Run matching
		PCGExPatternMatcher::FMatchResult Result = MatcherOperation->Match();

		if (Result.bSuccess)
		{
			// Run annotation (writes PatternName and MatchIndex attributes)
			MatcherOperation->Annotate(PatternNameWriter, PatternMatchIndexWriter);

			// Track annotated nodes for flag writing
			for (const FPCGExValencyPatternMatch& Match : MatcherOperation->GetMatches())
			{
				if (!Match.IsValid()) { continue; }

				// Skip unclaimed exclusive matches
				if (!Match.bClaimed)
				{
					const FPCGExValencyPatternCompiled& Pattern = Context->CompiledPatterns->Patterns[Match.PatternIndex];
					if (Pattern.Settings.bExclusive) { continue; }
				}

				const FPCGExValencyPatternCompiled& Pattern = Context->CompiledPatterns->Patterns[Match.PatternIndex];
				for (int32 EntryIdx = 0; EntryIdx < Match.EntryToNode.Num(); ++EntryIdx)
				{
					if (Pattern.Entries[EntryIdx].bIsActive)
					{
						AnnotatedNodes.Add(Match.EntryToNode[EntryIdx]);
					}
				}
			}
		}

		// Apply matches (topology-altering - to be moved to separate nodes in Phase 4)
		ApplyMatches();
	}

	void FProcessor::ApplyMatches()
	{
		if (!MatcherOperation) { return; }

		// Apply output strategies from matcher's matches
		// Note: Annotation is already done by the matcher operation during Annotate()
		// This method handles topology-altering operations (to be moved in Phase 4)
		for (const FPCGExValencyPatternMatch& Match : MatcherOperation->GetMatches())
		{
			if (!Match.IsValid()) { continue; }

			// Skip unclaimed exclusive matches
			if (!Match.bClaimed)
			{
				const FPCGExValencyPatternCompiled& Pattern = Context->CompiledPatterns->Patterns[Match.PatternIndex];
				if (Pattern.Settings.bExclusive) { continue; }
			}

			const FPCGExValencyPatternCompiled& Pattern = Context->CompiledPatterns->Patterns[Match.PatternIndex];
			const EPCGExPatternOutputStrategy Strategy = Pattern.Settings.OutputStrategy;

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
				// Already done by matcher operations, nothing more to do
				break;
			}
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

	void FProcessor::Write()
	{
		TProcessor::Write();

		// Helper to convert NodeIndex to PointIndex
		const TArray<PCGExClusters::FNode>& Nodes = *Cluster->Nodes;
		auto GetPointIdx = [&Nodes](int32 NodeIdx) -> int32
		{
			return Nodes.IsValidIndex(NodeIdx) ? Nodes[NodeIdx].PointIndex : -1;
		};

		// Apply collapse replacement transforms
		if (!CollapseReplacements.IsEmpty())
		{
			TPCGValueRange<FTransform> OutTransforms = VtxDataFacade->GetOut()->GetTransformValueRange();
			for (const auto& Pair : CollapseReplacements)
			{
				const int32 PointIdx = GetPointIdx(Pair.Key);
				if (PointIdx >= 0)
				{
					OutTransforms[PointIdx] = Pair.Value;
				}
			}
		}

		// Update ModuleData attribute with pattern flags
		if (ModuleDataWriter)
		{
			// First pass: Set Annotated flag for all matched nodes
			for (const int32 NodeIdx : AnnotatedNodes)
			{
				const int32 PointIdx = GetPointIdx(NodeIdx);
				if (PointIdx < 0) { continue; }
				int64 PackedData = ModuleDataReader->Read(PointIdx);
				PackedData = PCGExValency::ModuleData::SetFlag(PackedData, PCGExValency::ModuleData::Flags::Annotated);
				ModuleDataWriter->SetValue(PointIdx, PackedData);
			}

			// Second pass: Add Consumed flag for nodes being removed (preserves Annotated)
			for (const int32 NodeIdx : NodesToRemove)
			{
				const int32 PointIdx = GetPointIdx(NodeIdx);
				if (PointIdx < 0) { continue; }
				int64 PackedData = ModuleDataWriter->GetValue(PointIdx); // Read from writer to get Annotated flag
				PackedData = PCGExValency::ModuleData::SetFlag(PackedData, PCGExValency::ModuleData::Flags::Consumed);
				ModuleDataWriter->SetValue(PointIdx, PackedData);
			}

			// Add Collapsed flag for kept collapse nodes (preserves Annotated)
			for (const auto& Pair : CollapseReplacements)
			{
				const int32 PointIdx = GetPointIdx(Pair.Key);
				if (PointIdx < 0) { continue; }
				int64 PackedData = ModuleDataWriter->GetValue(PointIdx);
				PackedData = PCGExValency::ModuleData::SetFlag(PackedData, PCGExValency::ModuleData::Flags::Collapsed);
				ModuleDataWriter->SetValue(PointIdx, PackedData);
			}

			// Add Swapped flag and update module index (preserves Annotated)
			for (const auto& Pair : SwapTargets)
			{
				const int32 PointIdx = GetPointIdx(Pair.Key);
				if (PointIdx < 0) { continue; }
				int64 PackedData = ModuleDataWriter->GetValue(PointIdx);
				uint32 ExistingFlags = PCGExValency::ModuleData::GetFlags(PackedData);
				ExistingFlags |= PCGExValency::ModuleData::Flags::Swapped;
				PackedData = PCGExValency::ModuleData::Pack(Pair.Value, ExistingFlags);
				ModuleDataWriter->SetValue(PointIdx, PackedData);
			}
		}

		// Mark nodes for removal (actual removal happens in batch Write via point filtering)
		// The NodesToRemove set is used by the batch to filter points
	}

	// FBatch 

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

		// Register module data reader (int64, attribute name from OrbitalSet)
		const FPCGExValencyPatternReplacementContext* Context = GetContext<FPCGExValencyPatternReplacementContext>();
		if (Context && Context->OrbitalSet)
		{
			FacadePreloader.Register<int64>(ExecutionContext, Context->OrbitalSet->GetModuleIdxAttributeName());
		}

		// Register buffer dependencies from matcher factory
		if (Context && Context->MatcherFactory)
		{
			Context->MatcherFactory->RegisterPrimaryBuffersDependencies(ExecutionContext, FacadePreloader);
		}
	}

	void FBatch::OnProcessingPreparationComplete()
	{
		const UPCGExValencyPatternReplacementSettings* Settings = ExecutionContext->GetInputSettings<UPCGExValencyPatternReplacementSettings>();
		const FPCGExValencyPatternReplacementContext* Context = GetContext<FPCGExValencyPatternReplacementContext>();

		// Create module data reader/writer (packed int64 from Staging)
		if (Context && Context->OrbitalSet)
		{
			const FName ModuleAttrName = Context->OrbitalSet->GetModuleIdxAttributeName();
			ModuleDataReader = VtxDataFacade->GetReadable<int64>(ModuleAttrName);

			if (!ModuleDataReader)
			{
				PCGE_LOG_C(Error, GraphAndLog, Context,
				           FText::Format(FTEXT("Module attribute '{0}' not found. Run Valency Staging with patterns first."),
					           FText::FromName(ModuleAttrName)));
			}
			else
			{
				// Writer uses inherit mode since we're modifying existing Staging output
				const int64 DefaultValue = PCGExValency::ModuleData::Pack(PCGExValency::SlotState::UNSET);
				ModuleDataWriter = VtxDataFacade->GetWritable<int64>(ModuleAttrName, DefaultValue, true, PCGExData::EBufferInit::Inherit);
			}
		}

		// Create pattern name writer
		PatternNameWriter = VtxDataFacade->GetWritable<FName>(Settings->PatternNameAttributeName, FName(NAME_None), true, PCGExData::EBufferInit::New);

		// Create pattern match index writer
		PatternMatchIndexWriter = VtxDataFacade->GetWritable<int32>(Settings->PatternMatchIndexAttributeName, -1, true, PCGExData::EBufferInit::New);

		// Create matcher-specific allocations from factory
		if (Context && Context->MatcherFactory)
		{
			MatcherAllocations = Context->MatcherFactory->CreateAllocations(VtxDataFacade);
		}

		TBatch::OnProcessingPreparationComplete();
	}

	bool FBatch::PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor)
	{
		if (!TBatch::PrepareSingle(InProcessor)) { return false; }

		FProcessor* Processor = static_cast<FProcessor*>(InProcessor.Get());
		Processor->ModuleDataReader = ModuleDataReader;
		Processor->ModuleDataWriter = ModuleDataWriter;
		Processor->PatternNameWriter = PatternNameWriter;
		Processor->PatternMatchIndexWriter = PatternMatchIndexWriter;
		Processor->MatcherAllocations = MatcherAllocations;

		return true;
	}

	void FBatch::CompleteWork()
	{
		TBatch<FProcessor>::CompleteWork();
		VtxDataFacade->WriteFastest(TaskManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
