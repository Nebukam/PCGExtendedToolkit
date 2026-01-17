// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExValenceStaging.h"

#include "PCGParamData.h"
#include "Clusters/PCGExCluster.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGExData.h"
#include "Solvers/PCGExValenceEntropySolver.h"

#define LOCTEXT_NAMESPACE "PCGExValenceStaging"
#define PCGEX_NAMESPACE ValenceStaging

void UPCGExValenceStagingSettings::PostInitProperties()
{
	if (!HasAnyFlags(RF_ClassDefaultObject) && IsInGameThread())
	{
		if (!Solver) { Solver = NewObject<UPCGExValenceEntropySolver>(this, TEXT("Solver")); }
	}
	Super::PostInitProperties();
}

TArray<FPCGPinProperties> UPCGExValenceStagingSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAM(PCGExValence::Labels::SourceRulesetLabel, "Ruleset data asset override", Advanced)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExValenceStagingSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExValence::Labels::OutputStagedLabel, "Staged points with resolved module data", Required)
	return PinProperties;
}

PCGExData::EIOInit UPCGExValenceStagingSettings::GetMainOutputInitMode() const
{
	return PCGExData::EIOInit::Duplicate; // Duplicate since we're writing to vtx data
}

PCGExData::EIOInit UPCGExValenceStagingSettings::GetEdgeOutputInitMode() const
{
	return PCGExData::EIOInit::Forward;
}

PCGEX_ELEMENT_BATCH_EDGE_IMPL_ADV(ValenceStaging)

void FPCGExValenceStagingContext::RegisterAssetDependencies()
{
	FPCGExClustersProcessorContext::RegisterAssetDependencies();

	const UPCGExValenceStagingSettings* Settings = GetInputSettings<UPCGExValenceStagingSettings>();
	if (Settings && !Settings->Ruleset.IsNull())
	{
		AddAssetDependency(Settings->Ruleset.ToSoftObjectPath());
	}
}

FPCGElementPtr UPCGExValenceStagingSettings::CreateElement() const
{
	return MakeShared<FPCGExValenceStagingElement>();
}

bool FPCGExValenceStagingElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExClustersProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ValenceStaging)

	// Try to get ruleset from pin first, then from settings
	if (!Context->Ruleset)
	{
		if (!Settings->Ruleset.IsNull())
		{
			Context->Ruleset = Settings->Ruleset.LoadSynchronous();
		}
	}

	if (!Context->Ruleset)
	{
		if (!Settings->bQuietMissingRuleset)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("No Valence Ruleset provided."));
		}
		return false;
	}

	// Ensure ruleset is compiled
	if (!Context->Ruleset->IsCompiled())
	{
		if (!Context->Ruleset->Compile())
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Failed to compile Valence Ruleset."));
			return false;
		}
	}

	// Register solver from settings
	PCGEX_OPERATION_VALIDATE(Solver)
	Context->Solver = PCGEX_OPERATION_REGISTER_C(Context, UPCGExValenceSolverInstancedFactory, Settings->Solver, NAME_None);
	if (!Context->Solver) { return false; }

	return true;
}

void FPCGExValenceStagingElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
	FPCGExClustersProcessorElement::PostLoadAssetsDependencies(InContext);

	PCGEX_CONTEXT_AND_SETTINGS(ValenceStaging)

	if (!Context->Ruleset && !Settings->Ruleset.IsNull())
	{
		Context->Ruleset = Settings->Ruleset.Get();
	}
}

bool FPCGExValenceStagingElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	PCGEX_CONTEXT_AND_SETTINGS(ValenceStaging)

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

namespace PCGExValenceStaging
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExValenceStaging::Process);

		if (!TProcessor::Process(InTaskManager)) { return false; }

		// Build node contexts
		BuildNodeSlots();

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

	void FProcessor::BuildNodeSlots()
	{
		if (!Cluster) { return; }

		NodeSlots.SetNum(NumNodes);

		TArray<PCGExClusters::FNode>& Nodes = *Cluster->Nodes;

		// Build context for each node
		for (int32 NodeIndex = 0; NodeIndex < NumNodes; ++NodeIndex)
		{
			PCGExValence::FNodeSlot& NodeSlot = NodeSlots[NodeIndex];
			NodeSlot.NodeIndex = NodeIndex;

			const PCGExClusters::FNode& Node = Nodes[NodeIndex];

			// Get socket mask from attribute
			if (SocketMaskReader)
			{
				const int64 Mask = SocketMaskReader->Read(Node.PointIndex);
				NodeSlot.SocketMasks.Add(Mask);

				// Initialize socket-to-neighbor mapping
				NodeSlot.SocketToNeighbor.SetNum(64); // Max sockets
				for (int32 i = 0; i < 64; ++i)
				{
					NodeSlot.SocketToNeighbor[i] = -1; // Default no neighbor
				}
			}

			// Map links to sockets
			// For now, use link index as socket index (simple mapping)
			// In production, this should use the actual socket bit mapping from the ruleset
			for (int32 LinkIndex = 0; LinkIndex < Node.Links.Num(); ++LinkIndex)
			{
				const PCGExClusters::FLink& Link = Node.Links[LinkIndex];
				const int32 NeighborNodeIndex = Link.Node;

				// Map this neighbor to a socket
				// Simple approach: use link index as socket bit index
				if (LinkIndex < 64 && NodeSlot.SocketToNeighbor.IsValidIndex(LinkIndex))
				{
					NodeSlot.SocketToNeighbor[LinkIndex] = NeighborNodeIndex;
				}
			}
		}
	}

	void FProcessor::RunSolver()
	{
		if (!Context->Ruleset || !Context->Ruleset->CompiledData)
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

		Solver->Initialize(Context->Ruleset->CompiledData, NodeSlots, SolveSeed);
		SolveResult = Solver->Solve();

		if (SolveResult.UnsolvableCount > 0)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(FTEXT("Valence Solver: {0} nodes were unsolvable."), FText::AsNumber(SolveResult.UnsolvableCount)));
		}

		if (!SolveResult.MinimumsSatisfied)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Valence Solver: Minimum spawn constraints were not satisfied."));
		}
	}

	void FProcessor::WriteResults()
	{
		if (!Context->Ruleset || !Context->Ruleset->CompiledData)
		{
			return;
		}

		const UPCGExValenceRulesetCompiled* CompiledRuleset = Context->Ruleset->CompiledData;
		TArray<PCGExClusters::FNode>& Nodes = *Cluster->Nodes;

		for (const PCGExValence::FNodeSlot& NodeSlot : NodeSlots)
		{
			const PCGExClusters::FNode& Node = Nodes[NodeSlot.NodeIndex];

			// Write module index
			if (ModuleIndexWriter)
			{
				ModuleIndexWriter->SetValue(Node.PointIndex, NodeSlot.ResolvedModule);
			}

			// Write asset path
			if (AssetPathWriter && NodeSlot.ResolvedModule >= 0)
			{
				const TSoftObjectPtr<UObject>& Asset = CompiledRuleset->ModuleAssets[NodeSlot.ResolvedModule];
				AssetPathWriter->SetValue(Node.PointIndex, Asset.ToSoftObjectPath());
			}

			// Write unsolvable marker
			if (UnsolvableWriter)
			{
				UnsolvableWriter->SetValue(Node.PointIndex, NodeSlot.IsUnsolvable());
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

			for (const PCGExValence::FNodeSlot& NodeSlot : NodeSlots)
			{
				if (NodeSlot.IsUnsolvable())
				{
					const PCGExClusters::FNode& Node = Nodes[NodeSlot.NodeIndex];
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
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ValenceStaging)

		const TSharedRef<PCGExData::FFacade>& OutputFacade = VtxDataFacade;

		// Create socket mask reader
		const FName SocketMaskAttributeName = FName(Settings->LayerName.ToString() + TEXT("Mask"));
		SocketMaskReader = OutputFacade->GetBroadcaster<int64>(SocketMaskAttributeName);

		if (!SocketMaskReader)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(FTEXT("Socket mask attribute '{0}' not found on vertices."), FText::FromName(SocketMaskAttributeName)));
		}

		// Create writers
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

		FProcessor* TypedProcessor = static_cast<FProcessor*>(InProcessor.Get());

		// Forward reader and writers to processor
		TypedProcessor->SocketMaskReader = SocketMaskReader;
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
