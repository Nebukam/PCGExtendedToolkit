// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExValencyStaging.h"

#include "Core/PCGExCagePropertyCompiled.h"
#include "PCGParamData.h"
#include "Clusters/PCGExCluster.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGExData.h"
#include "Data/Utils/PCGExDataPreloader.h"
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

#if WITH_EDITOR
void UPCGExValencyStagingSettings::AutoPopulatePropertyOutputConfigs()
{
	// Load bonding rules if not already loaded
	UPCGExValencyBondingRules* LoadedRules = BondingRules.LoadSynchronous();
	if (!LoadedRules)
	{
		UE_LOG(LogTemp, Warning, TEXT("AutoPopulatePropertyOutputConfigs: No Bonding Rules set."));
		return;
	}

	// Compile if needed
	if (!LoadedRules->IsCompiled())
	{
		if (!LoadedRules->Compile())
		{
			UE_LOG(LogTemp, Warning, TEXT("AutoPopulatePropertyOutputConfigs: Failed to compile Bonding Rules."));
			return;
		}
	}

	const FPCGExValencyBondingRulesCompiled* CompiledRules = LoadedRules->CompiledData.Get();
	if (!CompiledRules)
	{
		UE_LOG(LogTemp, Warning, TEXT("AutoPopulatePropertyOutputConfigs: No compiled data available."));
		return;
	}

	const int32 AddedCount = PropertiesOutput.AutoPopulateFromRules(CompiledRules);

	if (AddedCount > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("AutoPopulatePropertyOutputConfigs: Added %d property output configs."), AddedCount);
		Modify();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("AutoPopulatePropertyOutputConfigs: No new properties found to add."));
	}
}
#endif

TArray<FPCGPinProperties> UPCGExValencyStagingSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAM(PCGExValency::Labels::SourceBondingRulesLabel, "Bonding rules data asset override", Advanced)
	if (bEnableFixedPicks) { PCGEX_PIN_FILTERS(PCGExValency::Labels::SourceFixedPickFiltersLabel, "Filters controlling which points are eligible for fixed picking.", Normal) }
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExValencyStagingSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExValency::Labels::OutputStagedLabel, "Staged points with resolved module data", Required)
	PCGEX_PIN_PARAMS(PCGExCollections::Labels::OutputCollectionMapLabel, "Collection map for resolving entry hashes", Required)
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
	FPCGExValencyProcessorContext::RegisterAssetDependencies();
	// Base class handles OrbitalSet and BondingRules registration via WantsOrbitalSet()/WantsBondingRules()
}

FPCGElementPtr UPCGExValencyStagingSettings::CreateElement() const
{
	return MakeShared<FPCGExValencyStagingElement>();
}

bool FPCGExValencyStagingElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExValencyProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ValencyStaging)

	PCGEX_OPERATION_VALIDATE(Solver)

	return true;
}

void FPCGExValencyStagingElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
	FPCGExValencyProcessorElement::PostLoadAssetsDependencies(InContext);
	// Base class handles OrbitalSet and BondingRules loading via WantsOrbitalSet()/WantsBondingRules()
}

bool FPCGExValencyStagingElement::PostBoot(FPCGExContext* InContext) const
{
	// Base class validates OrbitalSet and BondingRules via WantsOrbitalSet()/WantsBondingRules()
	if (!FPCGExValencyProcessorElement::PostBoot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ValencyStaging)

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

	// Settings->BondingRules->EDITOR_RegisterTrackingKeys(Context);

	// Register solver from settings
	Context->Solver = PCGEX_OPERATION_REGISTER_C(Context, UPCGExValencySolverInstancedFactory, Settings->Solver, NAME_None);
	if (!Context->Solver) { return false; }

	// Create pick packer for CollectionMap mode
	Context->PickPacker = MakeShared<PCGExCollections::FPickPacker>(Context);

	Context->MeshCollection = Context->BondingRules->GetMeshCollection();
	if (Context->MeshCollection) { Context->MeshCollection->BuildCache(); }

	Context->ActorCollection = Context->BondingRules->GetActorCollection();
	if (Context->ActorCollection) { Context->ActorCollection->BuildCache(); }

	// Get fixed pick filter factories if enabled (optional - empty array is valid)
	if (Settings->bEnableFixedPicks)
	{
		GetInputFactories(Context, PCGExValency::Labels::SourceFixedPickFiltersLabel, Context->FixedPickFilterFactories, PCGExFactories::ClusterNodeFilters, false);
	}

	return true;
}

bool FPCGExValencyStagingElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	PCGEX_CONTEXT_AND_SETTINGS(ValencyStaging)

	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
			{
				// Assign fixed pick filter factories to batch
				if (Settings->bEnableFixedPicks && !Context->FixedPickFilterFactories.IsEmpty())
				{
					static_cast<PCGExValencyStaging::FBatch*>(NewBatch.Get())->FixedPickFilterFactories = &Context->FixedPickFilterFactories;
				}
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->OutputPointsAndEdges();

	// Output collection map 
	UPCGParamData* ParamData = Context->ManagedObjects->New<UPCGParamData>();
	Context->PickPacker->PackToDataset(ParamData);
	Context->StageOutput(ParamData, PCGExCollections::Labels::OutputCollectionMapLabel, PCGExData::EStaging::None);

	return Context->TryComplete();
}

namespace PCGExValencyStaging
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExValencyStaging::Process);

		if (!TProcessor::Process(InTaskManager)) { return false; }

		// Initialize and run fixed pick filters if we have factories
		if (FixedPickFilterFactories && !FixedPickFilterFactories->IsEmpty() && FixedPickFilterCache)
		{
			FixedPickFiltersManager = MakeShared<PCGExClusterFilter::FManager>(Cluster.ToSharedRef(), VtxDataFacade, EdgeDataFacade);
			FixedPickFiltersManager->SetSupportedTypes(&PCGExFactories::ClusterNodeFilters);

			if (FixedPickFiltersManager->Init(ExecutionContext, *FixedPickFilterFactories))
			{
				// Run filters on all nodes to populate the cache
				const PCGExMT::FScope AllNodesScope(0, Cluster->Nodes->Num());
				FixedPickFiltersManager->Test(AllNodesScope.GetView(*Cluster->Nodes.Get()), *FixedPickFilterCache.Get(), true);
			}
		}

		// Apply fixed picks before solver runs (pre-resolve specified nodes)
		ApplyFixedPicks();

		// Run solver
		// BUG : Annotation are somehow broken when enabling local transform
		// TODO : Need to support wildcard for regular cages
		RunSolver();

		if (ValencyStates.IsEmpty()) { return false; }

		VALENCY_LOG_SECTION(Staging, "WRITING VALENCY RESULTS");

		if (!Context->BondingRules || !Context->BondingRules->CompiledData)
		{
			PCGEX_VALENCY_ERROR(Staging, "FProcessor::Process Missing BondingRules or CompiledData!");
			return false;
		}

		FittingHandler.ScaleToFit = Settings->ScaleToFit;
		FittingHandler.Justification = Settings->Justification;

		if (!FittingHandler.Init(ExecutionContext, VtxDataFacade)) { return false; }

		// Process valency states in parallel
		StartParallelLoopForRange(ValencyStates.Num());

		return true;
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		const FPCGExValencyBondingRulesCompiled* CompiledBondingRules = Context->BondingRules->CompiledData.Get();

		const TArray<PCGExClusters::FNode>& Nodes = *Cluster->Nodes.Get();

		TPCGValueRange<FTransform> OutTransforms = VtxDataFacade->GetOut()->GetTransformValueRange(false);
		TPCGValueRange<FVector> OutBoundsMin = VtxDataFacade->GetOut()->GetBoundsMinValueRange(false);
		TPCGValueRange<FVector> OutBoundsMax = VtxDataFacade->GetOut()->GetBoundsMaxValueRange(false);
		TConstPCGValueRange<int32> InSeeds = VtxDataFacade->GetIn()->GetConstSeedValueRange();

		PCGEX_SCOPE_LOOP(Index)
		{
			const PCGExValency::FValencyState& State = ValencyStates[Index];
			const PCGExClusters::FNode& Node = Nodes[State.NodeIndex];
			const int32 PointIndex = Node.PointIndex;

			// Write packed module data (module index + flags)
			if (ModuleDataWriter)
			{
				const int64 PackedData = PCGExValency::ModuleData::Pack(State.ResolvedModule);
				ModuleDataWriter->SetValue(PointIndex, PackedData);
			}

			if (State.ResolvedModule >= 0)
			{
				FPlatformAtomics::InterlockedIncrement(&ResolvedCount);
				const EPCGExValencyAssetType AssetType = CompiledBondingRules->ModuleAssetTypes[State.ResolvedModule];
				const FString AssetName = CompiledBondingRules->ModuleAssets[State.ResolvedModule].GetAssetName();

				// Write module name if enabled
				if (ModuleNameWriter)
				{
					ModuleNameWriter->SetValue(PointIndex, CompiledBondingRules->ModuleNames[State.ResolvedModule]);
				}

				// Write cage property outputs via helper
				if (PropertyWriter)
				{
					PropertyWriter->WriteModuleProperties(PointIndex, State.ResolvedModule);
				}

				PCGEX_VALENCY_VERBOSE(Staging, "  Node[%d] (Point=%d): Module[%d] = '%s' (Type=%d)", State.NodeIndex, PointIndex, State.ResolvedModule, *AssetName, static_cast<int32>(AssetType));

				if (EntryHashWriter && Context->PickPacker)
				{
					// Get the appropriate collection and entry index based on asset type
					const UPCGExAssetCollection* Collection = nullptr;
					FPCGExEntryAccessResult Result{};

					int32 EntryIndex = -1;
					int16 SecondaryIndex = -1;

					FTransform& OutTransform = OutTransforms[PointIndex];

					if (AssetType == EPCGExValencyAssetType::Mesh)
					{
						Collection = Context->MeshCollection;
						EntryIndex = Context->BondingRules->GetMeshEntryIndex(State.ResolvedModule);
						Result = Collection->GetEntryAt(EntryIndex);

						if (Result.IsValid())
						{
							if (const PCGExAssetCollection::FMicroCache* MicroCache = Result.Entry->MicroCache.Get())
							{
								SecondaryIndex = MicroCache->GetPickRandomWeighted(InSeeds[PointIndex]);
							}
						}
					}
					else if (AssetType == EPCGExValencyAssetType::Actor)
					{
						Collection = Context->ActorCollection;
						EntryIndex = Context->BondingRules->GetActorEntryIndex(State.ResolvedModule);
						Result = Collection->GetEntryAt(EntryIndex);
					}

					if (Collection && Result.IsValid())
					{
						const uint64 Hash = Context->PickPacker->GetPickIdx(Collection, EntryIndex, SecondaryIndex);
						EntryHashWriter->SetValue(PointIndex, static_cast<int64>(Hash));

						// Apply fitting
						FBox OutBounds = Result.Entry->Staging.Bounds;
						FVector Translation = FVector::ZeroVector;
						FittingHandler.ComputeTransform(PointIndex, OutTransform, OutBounds, Translation);						
						OutBoundsMin[Index] = OutBounds.Min;
						OutBoundsMax[Index] = OutBounds.Max;
						
						PCGEX_VALENCY_VERBOSE(Staging, "    -> EntryHash=0x%llX (EntryIndex=%d, SecondaryIndex=%d)", Hash, EntryIndex, SecondaryIndex);
					}
					else
					{
						PCGEX_VALENCY_WARNING(Staging, "    -> NO COLLECTION/ENTRY (Collection=%s, EntryIndex=%d)", Collection ? TEXT("Valid") : TEXT("NULL"), EntryIndex);
					}
					
					// Apply local transform if enabled (uses point seed to select among variants)
					if (Settings->bApplyLocalTransforms && CompiledBondingRules->ModuleHasLocalTransform[State.ResolvedModule])
					{
						const FTransform LocalTransform = CompiledBondingRules->GetModuleLocalTransform(State.ResolvedModule, InSeeds[PointIndex]);
						OutTransform = LocalTransform * OutTransform;
					}
				}
			}
			else if (State.IsUnsolvable())
			{
				FPlatformAtomics::InterlockedIncrement(&UnsolvableCount);
				PCGEX_VALENCY_VERBOSE(Staging, "  Node[%d] (Point=%d): UNSOLVABLE", State.NodeIndex, PointIndex);
			}
			else if (State.IsBoundary())
			{
				FPlatformAtomics::InterlockedIncrement(&BoundaryCount);
				PCGEX_VALENCY_VERBOSE(Staging, "  Node[%d] (Point=%d): BOUNDARY", State.NodeIndex, PointIndex);
			}

			// Write unsolvable marker
			if (UnsolvableWriter)
			{
				UnsolvableWriter->SetValue(PointIndex, State.IsUnsolvable());
			}
		}
	}

	void FProcessor::OnRangeProcessingComplete()
	{
		VALENCY_LOG_SECTION(Staging, "WRITE COMPLETE");
		PCGEX_VALENCY_INFO(Staging, "Resolved=%d, Unsolvable=%d, Boundary=%d", ResolvedCount, UnsolvableCount, BoundaryCount);
	}

	void FProcessor::ApplyFixedPicks()
	{
		// Skip if no fixed pick reader or no compiled data
		if (!FixedPickReader || !Context->BondingRules || !Context->BondingRules->CompiledData)
		{
			return;
		}

		const FPCGExValencyBondingRulesCompiled* CompiledRules = Context->BondingRules->CompiledData.Get();
		if (CompiledRules->ModuleCount == 0)
		{
			return;
		}

		VALENCY_LOG_SECTION(Staging, "APPLYING FIXED PICKS");

		// Build name to module indices map (once per processor)
		TMap<FName, TArray<int32>> NameToModules;
		for (int32 ModuleIndex = 0; ModuleIndex < CompiledRules->ModuleCount; ++ModuleIndex)
		{
			const FName& ModuleName = CompiledRules->ModuleNames[ModuleIndex];
			if (!ModuleName.IsNone())
			{
				NameToModules.FindOrAdd(ModuleName).Add(ModuleIndex);
			}
		}

		if (NameToModules.IsEmpty())
		{
			PCGEX_VALENCY_INFO(Staging, "No named modules found - skipping fixed picks");
			return;
		}

		PCGEX_VALENCY_INFO(Staging, "Found %d named module groups", NameToModules.Num());

		// Random stream for weighted selection (deterministic based on solver seed)
		int32 FixedPickSeed = Settings->Seed;
		if (Settings->bUsePerClusterSeed && Cluster)
		{
			FixedPickSeed = HashCombine(FixedPickSeed, GetTypeHash(VtxDataFacade->GetIn()->UID));
		}
		FRandomStream RandomStream(FixedPickSeed);

		int32 FixedPicksApplied = 0;
		int32 FixedPicksSkipped = 0;

		// Get cluster nodes for point index lookup
		const TArray<PCGExClusters::FNode>& Nodes = *Cluster->Nodes;

		// Apply fixed picks to states
		for (int32 StateIndex = 0; StateIndex < ValencyStates.Num(); ++StateIndex)
		{
			PCGExValency::FValencyState& State = ValencyStates[StateIndex];

			// Skip already resolved states (boundaries)
			if (State.IsResolved())
			{
				continue;
			}

			// Get the point index from cluster node
			const int32 PointIndex = Nodes[State.NodeIndex].PointIndex;

			// Read the fixed pick name for this node
			const FName PickName = FixedPickReader->Read(PointIndex);
			if (PickName.IsNone())
			{
				continue;
			}

			// Check FixedPickFilterCache if available (filter must pass for fixed pick to apply)
			if (FixedPickFilterCache && !(*FixedPickFilterCache)[PointIndex])
			{
				PCGEX_VALENCY_VERBOSE(Staging, "  State[%d]: Fixed pick '%s' skipped (filter failed)", StateIndex, *PickName.ToString());
				continue;
			}

			// Look up matching modules
			const TArray<int32>* MatchingModules = NameToModules.Find(PickName);
			if (!MatchingModules || MatchingModules->IsEmpty())
			{
				if (Settings->bWarnOnUnmatchedFixedPick)
				{
					PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(
						           FTEXT("Fixed pick '{0}' on node {1} doesn't match any module name."),
						           FText::FromName(PickName), FText::AsNumber(StateIndex)));
				}
				FixedPicksSkipped++;
				continue;
			}

			// Filter by orbital fit and select the best module
			int32 SelectedModule = -1;
			TArray<int32> FittingModules;

			// Get node's orbital mask from cache
			const int64 NodeOrbitalMask = OrbitalCache ? OrbitalCache->GetOrbitalMask(State.NodeIndex) : 0;

			for (int32 ModuleIndex : *MatchingModules)
			{
				// Check if module fits the node's orbital configuration
				bool bFits = true;
				for (int32 LayerIndex = 0; LayerIndex < CompiledRules->GetLayerCount(); ++LayerIndex)
				{
					const int64 ModuleMask = CompiledRules->GetModuleOrbitalMask(ModuleIndex, LayerIndex);
					const int64 ModuleBoundaryMask = CompiledRules->GetModuleBoundaryMask(ModuleIndex, LayerIndex);
					const int64 NodeMask = NodeOrbitalMask;

					// Module requires certain orbitals to be connected
					if ((ModuleMask & NodeMask) != ModuleMask)
					{
						bFits = false;
						break;
					}

					// Module requires certain orbitals to be disconnected (boundary)
					if ((ModuleBoundaryMask & NodeMask) != 0)
					{
						bFits = false;
						break;
					}
				}

				if (bFits)
				{
					FittingModules.Add(ModuleIndex);
				}
			}

			// Handle no fitting modules
			if (FittingModules.IsEmpty())
			{
				if (Settings->IncompatibleFixedPickBehavior == EPCGExFixedPickIncompatibleBehavior::Force)
				{
					// Force: use first matching module regardless of fit
					FittingModules = *MatchingModules;
					PCGEX_VALENCY_VERBOSE(Staging, "  State[%d]: Forcing fixed pick '%s' (incompatible orbital config)", StateIndex, *PickName.ToString());
				}
				else
				{
					// Skip: let solver decide
					if (Settings->bWarnOnIncompatibleFixedPick)
					{
						PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(
							           FTEXT("Fixed pick '{0}' on node {1} doesn't fit orbital configuration - skipping."),
							           FText::FromName(PickName), FText::AsNumber(StateIndex)));
					}
					FixedPicksSkipped++;
					continue;
				}
			}

			// Select from fitting modules based on selection mode
			if (FittingModules.Num() == 1)
			{
				SelectedModule = FittingModules[0];
			}
			else
			{
				switch (Settings->FixedPickSelectionMode)
				{
				case EPCGExFixedPickSelectionMode::FirstMatch:
					SelectedModule = FittingModules[0];
					break;

				case EPCGExFixedPickSelectionMode::BestFit:
					{
						// Select module with most matching orbitals
						int32 BestScore = -1;
						for (int32 ModuleIndex : FittingModules)
						{
							int32 Score = 0;
							for (int32 LayerIndex = 0; LayerIndex < CompiledRules->GetLayerCount(); ++LayerIndex)
							{
								const int64 ModuleMask = CompiledRules->GetModuleOrbitalMask(ModuleIndex, LayerIndex);
								Score += FMath::CountBits(ModuleMask & NodeOrbitalMask);
							}
							if (Score > BestScore)
							{
								BestScore = Score;
								SelectedModule = ModuleIndex;
							}
						}
					}
					break;

				case EPCGExFixedPickSelectionMode::WeightedRandom:
				default:
					{
						// Weighted random selection
						float TotalWeight = 0.0f;
						for (int32 ModuleIndex : FittingModules)
						{
							TotalWeight += CompiledRules->ModuleWeights[ModuleIndex];
						}

						if (TotalWeight > 0.0f)
						{
							float Pick = RandomStream.FRand() * TotalWeight;
							for (int32 ModuleIndex : FittingModules)
							{
								Pick -= CompiledRules->ModuleWeights[ModuleIndex];
								if (Pick <= 0.0f)
								{
									SelectedModule = ModuleIndex;
									break;
								}
							}
							// Fallback
							if (SelectedModule < 0)
							{
								SelectedModule = FittingModules.Last();
							}
						}
						else
						{
							// All weights zero, pick first
							SelectedModule = FittingModules[0];
						}
					}
					break;
				}
			}

			// Apply the fixed pick
			if (SelectedModule >= 0)
			{
				State.ResolvedModule = SelectedModule;
				FixedPicksApplied++;
				PCGEX_VALENCY_VERBOSE(Staging, "  State[%d]: Fixed pick '%s' -> Module[%d]", StateIndex, *PickName.ToString(), SelectedModule);
			}
		}

		PCGEX_VALENCY_INFO(Staging, "Fixed picks: %d applied, %d skipped", FixedPicksApplied, FixedPicksSkipped);
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

		Solver->Initialize(Context->BondingRules->CompiledData.Get(), ValencyStates, OrbitalCache.Get(), SolveSeed, SolverAllocations);
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

	void FBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		PCGExValencyMT::IBatch::RegisterBuffersDependencies(FacadePreloader);

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ValencyStaging)

		// Let solver register its buffer dependencies (e.g., priority attribute)
		if (Context->Solver)
		{
			Context->Solver->RegisterPrimaryBuffersDependencies(Context, FacadePreloader);
		}

		// Register fixed pick attribute if enabled
		if (Settings->bEnableFixedPicks)
		{
			FacadePreloader.TryRegister(Context, Settings->FixedPickAttribute);
		}
	}

	void FBatch::OnProcessingPreparationComplete()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ValencyStaging)

		EPCGPointNativeProperties PointAllocations = EPCGPointNativeProperties::Transform;

		//if (Settings->ScaleToFit.IsEnabled()){
		PointAllocations |= EPCGPointNativeProperties::BoundsMin;
		PointAllocations |= EPCGPointNativeProperties::BoundsMax;
		//}

		VtxDataFacade->GetOut()->AllocateProperties(PointAllocations);

		const TSharedRef<PCGExData::FFacade>& OutputFacade = VtxDataFacade;

		// Create solver allocations (buffers are now preloaded)
		if (Context->Solver)
		{
			SolverAllocations = Context->Solver->CreateAllocations(VtxDataFacade);
		}

		// Create staging-specific writers BEFORE calling base (base triggers PrepareSingle which forwards these)
		// Module index attribute name comes from OrbitalSet (PCGEx/V/MIdx/{LayerName})
		// Create Module data writer (int64: module index in low bits, pattern flags in high bits)
		// Only create if BondingRules has patterns defined
		if (Context->BondingRules && Context->BondingRules->CompiledData && Context->BondingRules->CompiledData->CompiledPatterns.HasPatterns())
		{
			const int64 DefaultValue = PCGExValency::ModuleData::Pack(PCGExValency::SlotState::UNSET);
			ModuleDataWriter = OutputFacade->GetWritable<int64>(Context->OrbitalSet->GetModuleIdxAttributeName(), DefaultValue, true, PCGExData::EBufferInit::Inherit);
		}

		// Write collection entry hash for downstream spawners
		EntryHashWriter = OutputFacade->GetWritable<int64>(PCGExCollections::Labels::Tag_EntryIdx, 0, true, PCGExData::EBufferInit::Inherit);

		if (Settings->bOutputUnsolvableMarker)
		{
			UnsolvableWriter = OutputFacade->GetWritable<bool>(Settings->UnsolvableAttributeName, false, true, PCGExData::EBufferInit::Inherit);
		}

		if (Settings->bOutputModuleName)
		{
			ModuleNameWriter = OutputFacade->GetWritable<FName>(Settings->ModuleNameAttributeName, NAME_None, true, PCGExData::EBufferInit::Inherit);
		}

		// Initialize property writer
		if (Context->BondingRules && Context->BondingRules->CompiledData)
		{
			PropertyWriter = MakeShared<FPCGExValencyPropertyWriter>();
			PropertyWriter->Initialize(
				Context->BondingRules->CompiledData.Get(),
				VtxDataFacade,
				Settings->PropertiesOutput);
		}

		// Get fixed pick reader and create filter cache if enabled
		if (Settings->bEnableFixedPicks)
		{
			FixedPickReader = VtxDataFacade->GetBroadcaster<FName>(Settings->FixedPickAttribute);

			// Create fixed pick filter cache
			FixedPickFilterCache = MakeShared<TArray<int8>>();
			FixedPickFilterCache->Init(Settings->bDefaultFixedPickFilterValue, VtxDataFacade->GetNum());

			// Register consumable attributes if we have filter factories
			if (FixedPickFilterFactories)
			{
				PCGExFactories::RegisterConsumableAttributesWithFacade(*FixedPickFilterFactories, VtxDataFacade);
			}
		}

		// Call base class AFTER creating writers (base triggers PrepareSingle)
		TBatch<FProcessor>::OnProcessingPreparationComplete();
	}

	bool FBatch::PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor)
	{
		// Call base class first - forwards orbital readers to processor
		// (Orbital cache is built by processor in Process() after cluster is available)
		if (!TBatch<FProcessor>::PrepareSingle(InProcessor)) { return false; }

		FProcessor* TypedProcessor = static_cast<FProcessor*>(InProcessor.Get());

		// Forward solver allocations to processor
		TypedProcessor->SolverAllocations = SolverAllocations;

		// Forward staging-specific writers to processor
		TypedProcessor->ModuleDataWriter = ModuleDataWriter;
		TypedProcessor->UnsolvableWriter = UnsolvableWriter;
		TypedProcessor->EntryHashWriter = EntryHashWriter;
		TypedProcessor->ModuleNameWriter = ModuleNameWriter;

		// Note: PropertyWriter is forwarded by base class in TBatch::PrepareSingle

		// Forward fixed pick reader, filter cache, and factories to processor
		TypedProcessor->FixedPickReader = FixedPickReader;
		TypedProcessor->FixedPickFilterCache = FixedPickFilterCache;
		TypedProcessor->FixedPickFilterFactories = FixedPickFilterFactories;

		return true;
	}

	void FBatch::CompleteWork()
	{
		VtxDataFacade->WriteFastest(TaskManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
