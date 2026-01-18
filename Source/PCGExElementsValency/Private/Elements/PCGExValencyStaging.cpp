// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExValencyStaging.h"

#include "PCGParamData.h"
#include "Clusters/PCGExCluster.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGExData.h"
#include "Helpers/PCGExCollectionsHelpers.h"
#include "PCGExCollectionsCommon.h"
#include "Collections/PCGExActorCollection.h"
#include "Collections/PCGExMeshCollection.h"
#include "Containers/PCGExManagedObjects.h"
#include "Solvers/PCGExValencyEntropySolver.h"
#include "Core/PCGExValencyLog.h"

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
	if (OutputMode == EPCGExStagingOutputMode::CollectionMap)
	{
		PCGEX_PIN_PARAMS(PCGExCollections::Labels::OutputCollectionMapLabel, "Collection map for resolving entry hashes", Required)
	}
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
	if (!Settings->BondingRules.IsNull())
	{
		Context->BondingRules = Settings->BondingRules.Get();
	}

	if (!Settings->OrbitalSet.IsNull())
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
		if (!Settings->bQuietMissingBondingRules) { PCGE_LOG(Error, GraphAndLog, FTEXT("Failed to load Valency Bonding Rules.")); }
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
		// TODO : Risky!
		if (!Context->BondingRules->Compile())
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Failed to compile Valency Bonding Rules."));
			return false;
		}
	}

	Settings->BondingRules->EDITOR_RegisterTrackingKeys(Context);
	Settings->OrbitalSet->EDITOR_RegisterTrackingKeys(Context);
	
	// Register solver from settings
	Context->Solver = PCGEX_OPERATION_REGISTER_C(Context, UPCGExValencySolverInstancedFactory, Settings->Solver, NAME_None);
	if (!Context->Solver) { return false; }

	// Create pick packer for CollectionMap mode
	if (Settings->OutputMode == EPCGExStagingOutputMode::CollectionMap)
	{
		Context->PickPacker = MakeShared<PCGExCollections::FPickPacker>(Context);
	}

	Context->MeshCollection = Context->BondingRules->GetMeshCollection();
	if (Context->MeshCollection) { Context->MeshCollection->BuildCache(); }

	Context->ActorCollection = Context->BondingRules->GetActorCollection();
	if (Context->ActorCollection) { Context->ActorCollection->BuildCache(); }

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

	// Output collection map if in CollectionMap mode
	if (Settings->OutputMode == EPCGExStagingOutputMode::CollectionMap && Context->PickPacker)
	{
		UPCGParamData* ParamData = Context->ManagedObjects->New<UPCGParamData>();
		Context->PickPacker->PackToDataset(ParamData);
		Context->StageOutput(ParamData, PCGExCollections::Labels::OutputCollectionMapLabel, PCGExData::EStaging::None);
	}

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

		VALENCY_LOG_SECTION(Staging, "BUILDING VALENCY STATES");
		PCGEX_VALENCY_INFO(Staging, "Cluster nodes: %d, MaxOrbitals: %d", NumNodes, Context->OrbitalSet->Num());

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

				// Log mask as binary
				FString MaskBits;
				for (int32 Bit = 0; Bit < MaxOrbitals; ++Bit)
				{
					MaskBits += (Mask & (1LL << Bit)) ? TEXT("1") : TEXT("0");
				}
				PCGEX_VALENCY_VERBOSE(Staging, "  Node[%d] (Point=%d): OrbitalMask=%s (0x%llX), Links=%d",
					NodeIndex, Node.PointIndex, *MaskBits, Mask, Node.Links.Num());
			}
			else
			{
				PCGEX_VALENCY_WARNING(Staging, "  Node[%d] (Point=%d): NO MASK READER, Links=%d",
					NodeIndex, Node.PointIndex, Node.Links.Num());
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
					PCGEX_VALENCY_VERBOSE(Staging, "    Link to Node[%d] via Edge[%d]: NO_ORBITAL_MATCH", NeighborNodeIndex, EdgeIndex);
					continue;
				}

				// Store neighbor at this orbital
				if (OrbitalIndex < MaxOrbitals)
				{
					State.OrbitalToNeighbor[OrbitalIndex] = NeighborNodeIndex;
					PCGEX_VALENCY_VERBOSE(Staging, "    Orbital[%d] -> Neighbor Node[%d]", OrbitalIndex, NeighborNodeIndex);
				}
			}
		}

		// Summary: count states with/without orbitals
		int32 WithOrbitals = 0;
		int32 WithoutOrbitals = 0;
		for (const PCGExValency::FValencyState& State : ValencyStates)
		{
			if (State.HasOrbitals()) { WithOrbitals++; } else { WithoutOrbitals++; }
		}
		VALENCY_LOG_SECTION(Staging, "STATE BUILDING COMPLETE");
		PCGEX_VALENCY_INFO(Staging, "With orbitals: %d, Without: %d", WithOrbitals, WithoutOrbitals);
	}

	void FProcessor::RunSolver()
	{
		VALENCY_LOG_SECTION(Staging, "RUNNING VALENCY SOLVER");

		if (!Context->BondingRules || !Context->BondingRules->CompiledData)
		{
			PCGEX_VALENCY_ERROR(Staging, "RunSolver: Missing BondingRules or CompiledData!");
			return;
		}

		PCGEX_VALENCY_INFO(Staging, "BondingRules: '%s', CompiledModules: %d",
			*Context->BondingRules->GetName(),
			Context->BondingRules->CompiledData->ModuleCount);

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

		PCGEX_VALENCY_INFO(Staging, "Initializing solver with seed %d, %d states", SolveSeed, ValencyStates.Num());

		Solver->Initialize(Context->BondingRules->CompiledData.Get(), ValencyStates, SolveSeed);
		SolveResult = Solver->Solve();

		VALENCY_LOG_SECTION(Staging, "SOLVER RESULT");
		PCGEX_VALENCY_INFO(Staging, "Resolved=%d, Unsolvable=%d, Boundary=%d, Success=%s",
			SolveResult.ResolvedCount, SolveResult.UnsolvableCount, SolveResult.BoundaryCount,
			SolveResult.bSuccess ? TEXT("true") : TEXT("false"));

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
		VALENCY_LOG_SECTION(Staging, "WRITING VALENCY RESULTS");

		if (!Context->BondingRules || !Context->BondingRules->CompiledData)
		{
			PCGEX_VALENCY_ERROR(Staging, "WriteResults: Missing BondingRules or CompiledData!");
			return;
		}

		const FPCGExValencyBondingRulesCompiled* CompiledBondingRules = Context->BondingRules->CompiledData.Get();
		TArray<PCGExClusters::FNode>& Nodes = *Cluster->Nodes;
		TPCGValueRange<FTransform> OutTransforms = VtxDataFacade->GetOut()->GetTransformValueRange();
		TConstPCGValueRange<int32> InSeeds = VtxDataFacade->GetIn()->GetConstSeedValueRange();

		int32 ResolvedCount = 0;
		int32 UnsolvableCount = 0;
		int32 BoundaryCount = 0;

		for (const PCGExValency::FValencyState& State : ValencyStates)
		{
			const PCGExClusters::FNode& Node = Nodes[State.NodeIndex];

			// Write module index
			if (ModuleIndexWriter)
			{
				ModuleIndexWriter->SetValue(Node.PointIndex, State.ResolvedModule);
			}

			if (State.ResolvedModule >= 0)
			{
				ResolvedCount++;
				const EPCGExValencyAssetType AssetType = CompiledBondingRules->ModuleAssetTypes[State.ResolvedModule];
				const FString AssetName = CompiledBondingRules->ModuleAssets[State.ResolvedModule].GetAssetName();

				PCGEX_VALENCY_VERBOSE(Staging, "  Node[%d] (Point=%d): Module[%d] = '%s' (Type=%d)",
					State.NodeIndex, Node.PointIndex, State.ResolvedModule, *AssetName, static_cast<int32>(AssetType));

				// Write asset path (Attributes mode) or entry hash (CollectionMap mode)
				if (AssetPathWriter)
				{
					const TSoftObjectPtr<UObject>& Asset = CompiledBondingRules->ModuleAssets[State.ResolvedModule];
					AssetPathWriter->SetValue(Node.PointIndex, Asset.ToSoftObjectPath());
				}
				else if (EntryHashWriter && Context->PickPacker)
				{
					// Get the appropriate collection and entry index based on asset type
					const UPCGExAssetCollection* Collection = nullptr;
					int32 EntryIndex = -1;
					int16 SecondaryIndex = -1;

					if (AssetType == EPCGExValencyAssetType::Mesh)
					{
						Collection = Context->MeshCollection;
						EntryIndex = Context->BondingRules->GetMeshEntryIndex(State.ResolvedModule);
						if (FPCGExEntryAccessResult Result = Collection->GetEntryAt(EntryIndex); Result.IsValid())
						{
							if (const PCGExAssetCollection::FMicroCache* MicroCache = Result.Entry->MicroCache.Get())
							{
								SecondaryIndex = MicroCache->GetPickRandomWeighted(InSeeds[Node.PointIndex]);
							}
						}
					}
					else if (AssetType == EPCGExValencyAssetType::Actor)
					{
						Collection = Context->ActorCollection;
						EntryIndex = Context->BondingRules->GetActorEntryIndex(State.ResolvedModule);
					}

					if (Collection && EntryIndex >= 0)
					{
						const uint64 Hash = Context->PickPacker->GetPickIdx(Collection, EntryIndex, SecondaryIndex);
						EntryHashWriter->SetValue(Node.PointIndex, static_cast<int64>(Hash));
						PCGEX_VALENCY_VERBOSE(Staging, "    -> EntryHash=0x%llX (EntryIndex=%d, SecondaryIndex=%d)",
							Hash, EntryIndex, SecondaryIndex);
					}
					else
					{
						PCGEX_VALENCY_WARNING(Staging, "    -> NO COLLECTION/ENTRY (Collection=%s, EntryIndex=%d)",
							Collection ? TEXT("Valid") : TEXT("NULL"), EntryIndex);
					}
				}

				// Apply local transform if enabled
				if (Settings->bApplyLocalTransforms && CompiledBondingRules->ModuleHasLocalTransform[State.ResolvedModule])
				{
					const FTransform& LocalTransform = CompiledBondingRules->ModuleLocalTransforms[State.ResolvedModule];
					const FTransform& CurrentTransform = OutTransforms[Node.PointIndex];
					OutTransforms[Node.PointIndex] = LocalTransform * CurrentTransform;
				}
			}
			else if (State.IsUnsolvable())
			{
				UnsolvableCount++;
				PCGEX_VALENCY_VERBOSE(Staging, "  Node[%d] (Point=%d): UNSOLVABLE", State.NodeIndex, Node.PointIndex);
			}
			else if (State.IsBoundary())
			{
				BoundaryCount++;
				PCGEX_VALENCY_VERBOSE(Staging, "  Node[%d] (Point=%d): BOUNDARY", State.NodeIndex, Node.PointIndex);
			}

			// Write unsolvable marker
			if (UnsolvableWriter)
			{
				UnsolvableWriter->SetValue(Node.PointIndex, State.IsUnsolvable());
			}
		}

		VALENCY_LOG_SECTION(Staging, "WRITE COMPLETE");
		PCGEX_VALENCY_INFO(Staging, "Resolved=%d, Unsolvable=%d, Boundary=%d", ResolvedCount, UnsolvableCount, BoundaryCount);
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

		if (Settings->OutputMode == EPCGExStagingOutputMode::Attributes)
		{
			// Write asset path directly to points
			AssetPathWriter = OutputFacade->GetWritable<FSoftObjectPath>(Settings->AssetPathAttributeName, FSoftObjectPath(), true, PCGExData::EBufferInit::Inherit);
		}
		else
		{
			// Write collection entry hash for downstream spawners
			const bool bInherit = OutputFacade->GetIn()->Metadata->HasAttribute(PCGExCollections::Labels::Tag_EntryIdx);
			EntryHashWriter = OutputFacade->GetWritable<int64>(PCGExCollections::Labels::Tag_EntryIdx, 0, true, bInherit ? PCGExData::EBufferInit::Inherit : PCGExData::EBufferInit::New);
		}

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
		TypedProcessor->EntryHashWriter = EntryHashWriter;

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
