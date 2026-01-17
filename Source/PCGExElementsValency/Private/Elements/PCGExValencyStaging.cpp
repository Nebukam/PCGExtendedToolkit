// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExValencyStaging.h"

#include "PCGParamData.h"
#include "Clusters/PCGExCluster.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGExData.h"
#include "Solvers/PCGExValencyEntropySolver.h"

#define LOCTEXT_NAMESPACE "PCGExValencyStaging"
#define PCGEX_NAMESPACE ValencyStaging

void UPCGExValencyStagingSettings::PostInitProperties()
{
	if (!HasAnyFlags(RF_ClassDefaultObject) && IsInGameThread())
	{
		if (!Solver) { Solver = NewObject<UPCGExValencyEntropySolver>(this, TEXT("Solver")); }
	}
	Super::PostInitProperties();
}

TArray<FPCGPinProperties> UPCGExValencyStagingSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAM(PCGExValency::Labels::SourceBondingRulesLabel, "Bonding rules data asset override", Advanced)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExValencyStagingSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExValency::Labels::OutputStagedLabel, "Staged points with resolved module data", Required)
	return PinProperties;
}

PCGExData::EIOInit UPCGExValencyStagingSettings::GetMainOutputInitMode() const
{
	return PCGExData::EIOInit::Duplicate; // Duplicate since we're writing to vtx data
}

PCGExData::EIOInit UPCGExValencyStagingSettings::GetEdgeOutputInitMode() const
{
	return PCGExData::EIOInit::Forward;
}

PCGEX_ELEMENT_BATCH_EDGE_IMPL_ADV(ValencyStaging)

void FPCGExValencyStagingContext::RegisterAssetDependencies()
{
	FPCGExClustersProcessorContext::RegisterAssetDependencies();

	const UPCGExValencyStagingSettings* Settings = GetInputSettings<UPCGExValencyStagingSettings>();
	if (Settings)
	{
		if (!Settings->BondingRules.IsNull())
		{
			AddAssetDependency(Settings->BondingRules.ToSoftObjectPath());
		}
		if (!Settings->OrbitalSet.IsNull())
		{
			AddAssetDependency(Settings->OrbitalSet.ToSoftObjectPath());
		}
	}
}

FPCGElementPtr UPCGExValencyStagingSettings::CreateElement() const
{
	return MakeShared<FPCGExValencyStagingElement>();
}

bool FPCGExValencyStagingElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExClustersProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ValencyStaging)

	// Validate solver settings (doesn't require loaded assets)
	PCGEX_OPERATION_VALIDATE(Solver)

	// Check that asset references are provided (but don't load them yet)
	if (Settings->BondingRules.IsNull())
	{
		if (!Settings->bQuietMissingBondingRules)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("No Valency Bonding Rules provided."));
		}
		return false;
	}

	if (Settings->OrbitalSet.IsNull())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("No Valency Orbital Set provided."));
		return false;
	}

	return true;
}

void FPCGExValencyStagingElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
	FPCGExClustersProcessorElement::PostLoadAssetsDependencies(InContext);

	PCGEX_CONTEXT_AND_SETTINGS(ValencyStaging)

	// Get loaded assets
	if (!Context->BondingRules && !Settings->BondingRules.IsNull())
	{
		Context->BondingRules = Settings->BondingRules.Get();
	}

	if (!Context->OrbitalSet && !Settings->OrbitalSet.IsNull())
	{
		Context->OrbitalSet = Settings->OrbitalSet.Get();
	}
}

bool FPCGExValencyStagingElement::PostBoot(FPCGExContext* InContext) const
{
	if (!FPCGExClustersProcessorElement::PostBoot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ValencyStaging)

	// Validate loaded assets
	if (!Context->BondingRules)
	{
		if (!Settings->bQuietMissingBondingRules)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Failed to load Valency Bonding Rules."));
		}
		return false;
	}

	if (!Context->OrbitalSet)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Failed to load Valency Orbital Set."));
		return false;
	}

	// Ensure bonding rules are compiled
	if (!Context->BondingRules->IsCompiled())
	{
		if (!Context->BondingRules->Compile())
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Failed to compile Valency Bonding Rules."));
			return false;
		}
	}

	// Register solver from settings
	Context->Solver = PCGEX_OPERATION_REGISTER_C(Context, UPCGExValencySolverInstancedFactory, Settings->Solver, NAME_None);
	if (!Context->Solver) { return false; }

	return true;
}

bool FPCGExValencyStagingElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	PCGEX_CONTEXT_AND_SETTINGS(ValencyStaging)

	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters([](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; }, [&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
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

namespace PCGExValencyStaging
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExValencyStaging::Process);

		if (!TProcessor::Process(InTaskManager)) { return false; }

		// Create edge indices reader for this processor's edge facade
		const FName IdxAttributeName = Context->OrbitalSet->GetOrbitalIdxAttributeName();
		EdgeIndicesReader = EdgeDataFacade->GetReadable<int64>(Context->OrbitalSet->GetOrbitalIdxAttributeName());

		if (!EdgeIndicesReader)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FTEXT("Edge indices attribute '{0}' not found on edges. Run 'Write Valency Orbitals' first."), FText::FromName(IdxAttributeName)));
			return false;
		}

		// Build valency states from pre-computed attributes
		BuildValencyStates();

		// Run solver
		RunSolver();

		// Write results (writers are forwarded from batch)
		WriteResults();

		return true;
	}

	void FProcessor::ProcessNodes(const PCGExMT::FScope& Scope)
	{
		// Not used - we do everything in Process for now
	}

	void FProcessor::OnNodesProcessingComplete()
	{
		// Not used
	}

	void FProcessor::BuildValencyStates()
	{
		if (!Cluster || !Context->OrbitalSet) { return; }

		ValencyStates.SetNum(NumNodes);

		TArray<PCGExClusters::FNode>& Nodes = *Cluster->Nodes;
		const int32 MaxOrbitals = Context->OrbitalSet->Num();

		// Build state for each node
		for (int32 NodeIndex = 0; NodeIndex < NumNodes; ++NodeIndex)
		{
			PCGExValency::FValencyState& State = ValencyStates[NodeIndex];
			State.NodeIndex = NodeIndex;

			const PCGExClusters::FNode& Node = Nodes[NodeIndex];

			// Read orbital mask from pre-computed vertex attribute
			if (OrbitalMaskReader)
			{
				const int64 Mask = OrbitalMaskReader->Read(Node.PointIndex);
				State.OrbitalMasks.Add(Mask);
			}

			// Initialize orbital-to-neighbor mapping with no neighbors
			State.OrbitalToNeighbor.SetNum(MaxOrbitals);
			for (int32 i = 0; i < MaxOrbitals; ++i)
			{
				State.OrbitalToNeighbor[i] = -1;
			}

			// Build orbital-to-neighbor from edge indices
			for (const PCGExClusters::FLink& Link : Node.Links)
			{
				const int32 EdgeIndex = Link.Edge;
				const int32 NeighborNodeIndex = Link.Node;

				if (!EdgeIndicesReader || !Cluster->Edges->IsValidIndex(EdgeIndex))
				{
					continue;
				}

				const PCGExGraphs::FEdge& Edge = (*Cluster->Edges)[EdgeIndex];
				const int64 PackedIndices = EdgeIndicesReader->Read(EdgeIndex);

				// Unpack orbital indices (byte 0 = start, byte 1 = end)
				const uint8 StartOrbitalIndex = static_cast<uint8>(PackedIndices & 0xFF);
				const uint8 EndOrbitalIndex = static_cast<uint8>((PackedIndices >> 8) & 0xFF);

				// Determine which orbital index applies to this node
				uint8 OrbitalIndex;
				if (Edge.Start == Node.PointIndex)
				{
					OrbitalIndex = StartOrbitalIndex;
				}
				else
				{
					OrbitalIndex = EndOrbitalIndex;
				}

				// Skip if no match (sentinel value)
				if (OrbitalIndex == PCGExValency::NO_ORBITAL_MATCH)
				{
					continue;
				}

				// Store neighbor at this orbital
				if (OrbitalIndex < MaxOrbitals)
				{
					State.OrbitalToNeighbor[OrbitalIndex] = NeighborNodeIndex;
				}
			}
		}
	}

	void FProcessor::RunSolver()
	{
		if (!Context->BondingRules || !Context->BondingRules->CompiledData)
		{
			return;
		}

		// Create solver from factory
		if (Context->Solver)
		{
			Solver = Context->Solver->CreateOperation();
		}

		if (!Solver)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Failed to create solver."));
			return;
		}

		// Calculate seed
		int32 SolveSeed = Settings->Seed;
		if (Settings->bUsePerClusterSeed && Cluster)
		{
			// Mix in cluster-specific data for variation
			SolveSeed = HashCombine(SolveSeed, GetTypeHash(VtxDataFacade->GetIn()->UID));
		}

		Solver->Initialize(Context->BondingRules->CompiledData, ValencyStates, SolveSeed);
		SolveResult = Solver->Solve();

		if (SolveResult.UnsolvableCount > 0)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(FTEXT("Valency Solver: {0} nodes were unsolvable."), FText::AsNumber(SolveResult.UnsolvableCount)));
		}

		if (!SolveResult.MinimumsSatisfied)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Valency Solver: Minimum spawn constraints were not satisfied."));
		}
	}

	void FProcessor::WriteResults()
	{
		if (!Context->BondingRules || !Context->BondingRules->CompiledData)
		{
			return;
		}

		const UPCGExValencyBondingRulesCompiled* CompiledBondingRules = Context->BondingRules->CompiledData;
		TArray<PCGExClusters::FNode>& Nodes = *Cluster->Nodes;

		for (const PCGExValency::FValencyState& State : ValencyStates)
		{
			const PCGExClusters::FNode& Node = Nodes[State.NodeIndex];

			// Write module index
			if (ModuleIndexWriter)
			{
				ModuleIndexWriter->SetValue(Node.PointIndex, State.ResolvedModule);
			}

			// Write asset path
			if (AssetPathWriter && State.ResolvedModule >= 0)
			{
				const TSoftObjectPtr<UObject>& Asset = CompiledBondingRules->ModuleAssets[State.ResolvedModule];
				AssetPathWriter->SetValue(Node.PointIndex, Asset.ToSoftObjectPath());
			}

			// Write unsolvable marker
			if (UnsolvableWriter)
			{
				UnsolvableWriter->SetValue(Node.PointIndex, State.IsUnsolvable());
			}
		}
	}

	void FProcessor::Write()
	{
		TProcessor::Write();

		// Optionally prune unsolvable points
		if (Settings->bPruneUnsolvable)
		{
			TArray<PCGExClusters::FNode>& Nodes = *Cluster->Nodes;
			TArray<int32> IndicesToRemove;

			for (const PCGExValency::FValencyState& State : ValencyStates)
			{
				if (State.IsUnsolvable())
				{
					const PCGExClusters::FNode& Node = Nodes[State.NodeIndex];
					IndicesToRemove.Add(Node.PointIndex);
				}
			}

			// Note: Actual point removal would need to be handled by the cluster system
			// This is a placeholder for the pruning logic
		}
	}

	//////// BATCH

	FBatch::FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
		: TBatch(InContext, InVtx, InEdges)
	{
	}

	FBatch::~FBatch()
	{
	}

	void FBatch::OnProcessingPreparationComplete()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ValencyStaging)

		const TSharedRef<PCGExData::FFacade>& OutputFacade = VtxDataFacade;

		// Get attribute names from orbital set
		const FName MaskAttributeName = Context->OrbitalSet->GetOrbitalMaskAttributeName();

		// Create orbital mask reader (vertex attribute)
		OrbitalMaskReader = OutputFacade->GetReadable<int64>(MaskAttributeName);

		if (!OrbitalMaskReader)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(FTEXT("Orbital mask attribute '{0}' not found on vertices. Run 'Write Valency Orbitals' first."), FText::FromName(MaskAttributeName)));
			bIsBatchValid = false;
			return;
		}

		// Create edge indices reader (edge attribute) - need to get from edge facade
		// Note: Edge reading is handled per-processor since each cluster has its own edges

		// Create writers; inherit in case we run with a different layer
		ModuleIndexWriter = OutputFacade->GetWritable<int32>(Settings->ModuleIndexAttributeName, -1, true, PCGExData::EBufferInit::Inherit);
		AssetPathWriter = OutputFacade->GetWritable<FSoftObjectPath>(Settings->AssetPathAttributeName, FSoftObjectPath(), true, PCGExData::EBufferInit::Inherit);

		if (Settings->bOutputUnsolvableMarker)
		{
			UnsolvableWriter = OutputFacade->GetWritable<bool>(Settings->UnsolvableAttributeName, false, true, PCGExData::EBufferInit::Inherit);
		}

		TBatch<FProcessor>::OnProcessingPreparationComplete();
	}

	bool FBatch::PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor)
	{
		if (!TBatch<FProcessor>::PrepareSingle(InProcessor)) { return false; }

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ValencyStaging)

		FProcessor* TypedProcessor = static_cast<FProcessor*>(InProcessor.Get());

		// Forward reader and writers to processor
		TypedProcessor->OrbitalMaskReader = OrbitalMaskReader;
		TypedProcessor->ModuleIndexWriter = ModuleIndexWriter;
		TypedProcessor->AssetPathWriter = AssetPathWriter;
		TypedProcessor->UnsolvableWriter = UnsolvableWriter;

		return true;
	}

	void FBatch::Write()
	{
		VtxDataFacade->WriteFastest(TaskManager);
		TBatch<FProcessor>::Write();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
