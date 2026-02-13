// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExValencyGenerative.h"

#include "PCGParamData.h"
#include "Data/PCGExData.h"
#include "Helpers/PCGExCollectionsHelpers.h"
#include "Helpers/PCGExPointArrayDataHelpers.h"
#include "Helpers/PCGExStreamingHelpers.h"
#include "PCGExCollectionsCommon.h"
#include "Collections/PCGExActorCollection.h"
#include "Collections/PCGExMeshCollection.h"
#include "Containers/PCGExManagedObjects.h"
#include "Data/PCGExPointIO.h"
#include "Growth/PCGExValencyGrowthBFS.h"

#define LOCTEXT_NAMESPACE "PCGExValencyGenerative"
#define PCGEX_NAMESPACE ValencyGenerative

PCGEX_INITIALIZE_ELEMENT(ValencyGenerative)
PCGEX_ELEMENT_BATCH_POINT_IMPL(ValencyGenerative)

#pragma region UPCGExValencyGenerativeSettings

void UPCGExValencyGenerativeSettings::PostInitProperties()
{
	if (!HasAnyFlags(RF_ClassDefaultObject) && IsInGameThread())
	{
		if (!GrowthStrategy) { GrowthStrategy = NewObject<UPCGExValencyGrowthBFSFactory>(this, TEXT("GrowthStrategy")); }
	}
	Super::PostInitProperties();
}

TArray<FPCGPinProperties> UPCGExValencyGenerativeSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_PARAMS(PCGExCollections::Labels::OutputCollectionMapLabel, "Collection map for resolving entry hashes", Required)
	return PinProperties;
}

#pragma endregion

#pragma region FPCGExValencyGenerativeContext

void FPCGExValencyGenerativeContext::RegisterAssetDependencies()
{
	FPCGExPointsProcessorContext::RegisterAssetDependencies();

	const UPCGExValencyGenerativeSettings* Settings = GetInputSettings<UPCGExValencyGenerativeSettings>();
	if (!Settings) { return; }

	if (!Settings->BondingRules.IsNull())
	{
		AddAssetDependency(Settings->BondingRules.ToSoftObjectPath());
	}

	if (!Settings->ConnectorSet.IsNull())
	{
		AddAssetDependency(Settings->ConnectorSet.ToSoftObjectPath());
	}
}

#pragma endregion

#pragma region FPCGExValencyGenerativeElement

bool FPCGExValencyGenerativeElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ValencyGenerative)

	// Validate required settings
	if (Settings->BondingRules.IsNull())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("No Bonding Rules provided."));
		return false;
	}

	if (Settings->ConnectorSet.IsNull())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("No Connector Set provided."));
		return false;
	}

	PCGEX_OPERATION_VALIDATE(GrowthStrategy)

	// Load assets
	PCGExHelpers::LoadBlocking_AnyThreadTpl(Settings->BondingRules, InContext);
	PCGExHelpers::LoadBlocking_AnyThreadTpl(Settings->ConnectorSet, InContext);

	return true;
}

void FPCGExValencyGenerativeElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
	FPCGExPointsProcessorElement::PostLoadAssetsDependencies(InContext);

	PCGEX_CONTEXT_AND_SETTINGS(ValencyGenerative)

	Context->BondingRules = Settings->BondingRules.Get();
	Context->ConnectorSet = Settings->ConnectorSet.Get();
}

bool FPCGExValencyGenerativeElement::PostBoot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::PostBoot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ValencyGenerative)

	if (!Context->BondingRules)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Failed to load Bonding Rules."));
		return false;
	}

	if (!Context->ConnectorSet)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Failed to load Connector Set."));
		return false;
	}

	// Ensure bonding rules are compiled
	if (!Context->BondingRules->IsCompiled())
	{
		if (!Context->BondingRules->Compile())
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Failed to compile Bonding Rules."));
			return false;
		}
	}

	// Register growth factory
	Context->GrowthFactory = PCGEX_OPERATION_REGISTER_C(Context, UPCGExValencyGrowthFactory, Settings->GrowthStrategy, NAME_None);
	if (!Context->GrowthFactory) { return false; }

	// Create pick packer
	Context->PickPacker = MakeShared<PCGExCollections::FPickPacker>(Context);

	// Get collections
	Context->MeshCollection = Context->BondingRules->GetMeshCollection();
	if (Context->MeshCollection) { Context->MeshCollection->BuildCache(); }

	Context->ActorCollection = Context->BondingRules->GetActorCollection();
	if (Context->ActorCollection) { Context->ActorCollection->BuildCache(); }

	// Cache compiled rules
	Context->CompiledRules = Context->BondingRules->GetCompiledData();
	if (!Context->CompiledRules || Context->CompiledRules->ModuleCount == 0)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("No compiled modules in bonding rules."));
		return false;
	}

	// Compile connector set
	Context->ConnectorSet->Compile();

	// Build module local bounds cache from collection staging data
	const int32 ModuleCount = Context->CompiledRules->ModuleCount;
	Context->ModuleLocalBounds.SetNum(ModuleCount);
	for (int32 i = 0; i < ModuleCount; ++i)
	{
		Context->ModuleLocalBounds[i] = FBox(ForceInit);
	}

	// Populate bounds from mesh collection entries
	if (Context->MeshCollection)
	{
		for (int32 ModuleIdx = 0; ModuleIdx < ModuleCount; ++ModuleIdx)
		{
			const int32 EntryIndex = Context->BondingRules->GetMeshEntryIndex(ModuleIdx);
			if (EntryIndex >= 0)
			{
				const FPCGExEntryAccessResult Result = Context->MeshCollection->GetEntryRaw(EntryIndex);
				if (Result.IsValid())
				{
					Context->ModuleLocalBounds[ModuleIdx] = Result.Entry->Staging.Bounds;

					if (!FMath::IsNearlyZero(Settings->BoundsInflation))
					{
						Context->ModuleLocalBounds[ModuleIdx] = Context->ModuleLocalBounds[ModuleIdx].ExpandBy(Settings->BoundsInflation);
					}
				}
			}
		}
	}

	// Populate bounds from actor collection entries
	if (Context->ActorCollection)
	{
		for (int32 ModuleIdx = 0; ModuleIdx < ModuleCount; ++ModuleIdx)
		{
			const int32 EntryIndex = Context->BondingRules->GetActorEntryIndex(ModuleIdx);
			if (EntryIndex >= 0)
			{
				const FPCGExEntryAccessResult Result = Context->ActorCollection->GetEntryRaw(EntryIndex);
				if (Result.IsValid())
				{
					Context->ModuleLocalBounds[ModuleIdx] = Result.Entry->Staging.Bounds;

					if (!FMath::IsNearlyZero(Settings->BoundsInflation))
					{
						Context->ModuleLocalBounds[ModuleIdx] = Context->ModuleLocalBounds[ModuleIdx].ExpandBy(Settings->BoundsInflation);
					}
				}
			}
		}
	}

	// Build name-to-module lookup for seed filtering
	for (int32 ModuleIdx = 0; ModuleIdx < ModuleCount; ++ModuleIdx)
	{
		const FName& ModuleName = Context->CompiledRules->ModuleNames[ModuleIdx];
		if (!ModuleName.IsNone())
		{
			Context->NameToModules.FindOrAdd(ModuleName).Add(ModuleIdx);
		}
	}

	return true;
}

bool FPCGExValencyGenerativeElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	PCGEX_CONTEXT_AND_SETTINGS(ValencyGenerative)
	PCGEX_EXECUTION_CHECK

	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("No seed points provided."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	// Output all processor-created IOs
	Context->MainBatch->Output();

	// Output collection map
	UPCGParamData* ParamData = Context->ManagedObjects->New<UPCGParamData>();
	Context->PickPacker->PackToDataset(ParamData);

	FPCGTaggedData& OutData = Context->OutputData.TaggedData.Emplace_GetRef();
	OutData.Pin = PCGExCollections::Labels::OutputCollectionMapLabel;
	OutData.Data = ParamData;

	return Context->TryComplete();
}

#pragma endregion

#pragma region PCGExValencyGenerative::FProcessor

namespace PCGExValencyGenerative
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExValencyGenerative::Process);

		if (!IProcessor::Process(InTaskManager)) { return false; }

		const int32 NumSeeds = PointDataFacade->GetNum();
		if (NumSeeds == 0) { return false; }

		// Allocate resolved module array
		ResolvedModules.SetNumUninitialized(NumSeeds);
		for (int32 i = 0; i < NumSeeds; ++i) { ResolvedModules[i] = -1; }

		// Prepare name attribute reader for seed filtering
		if (!Settings->SeedModuleNameAttribute.IsNone())
		{
			NameReader = PointDataFacade->GetReadable<FName>(Settings->SeedModuleNameAttribute);
		}

		// Create growth operation
		GrowthOp = Context->GrowthFactory->CreateOperation();
		if (!GrowthOp) { return false; }

		StartParallelLoopForPoints(PCGExData::EIOSide::In);

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExValencyGenerative::ProcessPoints);

		PointDataFacade->Fetch(Scope);

		const FPCGExValencyBondingRulesCompiled* CompiledRules = Context->CompiledRules;
		const TMap<FName, TArray<int32>>& NameToModules = Context->NameToModules;

		PCGEX_SCOPE_LOOP(Index)
		{
			// Determine candidate modules for this seed
			TArray<int32> CandidateModules;

			if (NameReader)
			{
				const FName RequestedName = NameReader->Read(Index);
				if (!RequestedName.IsNone())
				{
					if (const TArray<int32>* Matching = NameToModules.Find(RequestedName))
					{
						CandidateModules = *Matching;
					}
				}
			}

			// If no filtering or no match, use all modules that have connectors
			if (CandidateModules.IsEmpty())
			{
				for (int32 ModuleIdx = 0; ModuleIdx < CompiledRules->ModuleCount; ++ModuleIdx)
				{
					if (CompiledRules->GetModuleConnectorCount(ModuleIdx) > 0)
					{
						CandidateModules.Add(ModuleIdx);
					}
				}
			}

			if (CandidateModules.IsEmpty())
			{
				ResolvedModules[Index] = -1;
				continue;
			}

			// Weighted random selection using per-point deterministic seed
			float TotalWeight = 0.0f;
			for (const int32 ModuleIdx : CandidateModules)
			{
				TotalWeight += CompiledRules->ModuleWeights[ModuleIdx];
			}

			int32 SelectedModule = CandidateModules[0];
			if (TotalWeight > 0.0f)
			{
				FRandomStream PointRandom(HashCombine(Settings->Seed, Index));
				float Pick = PointRandom.FRand() * TotalWeight;
				for (const int32 ModuleIdx : CandidateModules)
				{
					Pick -= CompiledRules->ModuleWeights[ModuleIdx];
					if (Pick <= 0.0f)
					{
						SelectedModule = ModuleIdx;
						break;
					}
				}
			}

			ResolvedModules[Index] = SelectedModule;
		}
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		const FPCGExValencyBondingRulesCompiled* CompiledRules = Context->CompiledRules;

		// Setup budget and bounds tracker for this dataset
		FPCGExGrowthBudget Budget = Settings->Budget;
		Budget.Reset();

		FPCGExBoundsTracker BoundsTracker;

		// Initialize growth operation with per-dataset state
		GrowthOp->Initialize(CompiledRules, Context->ConnectorSet, BoundsTracker, Budget, Settings->Seed);
		GrowthOp->ModuleLocalBounds = Context->ModuleLocalBounds;

		// Build placed module entries from resolved seeds
		const int32 NumSeeds = PointDataFacade->GetNum();
		TConstPCGValueRange<FTransform> SeedTransforms = PointDataFacade->GetIn()->GetConstTransformValueRange();

		int32 SeedIdx = 0;
		for (int32 Index = 0; Index < NumSeeds; ++Index)
		{
			const int32 SelectedModule = ResolvedModules[Index];
			if (SelectedModule < 0) { continue; }

			FPCGExPlacedModule SeedModule;
			SeedModule.ModuleIndex = SelectedModule;
			SeedModule.WorldTransform = SeedTransforms[Index];
			SeedModule.WorldBounds = GrowthOp->ComputeWorldBounds(SelectedModule, SeedModule.WorldTransform);
			SeedModule.ParentIndex = -1;
			SeedModule.ParentConnectorIndex = -1;
			SeedModule.ChildConnectorIndex = -1;
			SeedModule.Depth = 0;
			SeedModule.SeedIndex = SeedIdx;
			SeedModule.CumulativeWeight = CompiledRules->ModuleWeights[SelectedModule];

			PlacedModules.Add(SeedModule);
			Budget.CurrentTotal++;

			if (SeedModule.WorldBounds.IsValid)
			{
				BoundsTracker.Add(SeedModule.WorldBounds);
			}

			SeedIdx++;
		}

		if (PlacedModules.IsEmpty()) { return; }

		// Run the growth (sequential)
		GrowthOp->Grow(PlacedModules);

		// Create output point data
		OutputIO = PCGExData::NewPointIO(Context, PCGPinConstants::DefaultOutputLabel);
		UPCGBasePointData* OutPointData = OutputIO->GetOut();

		const int32 TotalPlaced = PlacedModules.Num();

		// Allocate points with transform + bounds
		const EPCGPointNativeProperties AllocatedProperties = EPCGPointNativeProperties::Transform | EPCGPointNativeProperties::BoundsMin | EPCGPointNativeProperties::BoundsMax | EPCGPointNativeProperties::Seed;
		PCGExPointArrayDataHelpers::SetNumPointsAllocated(OutPointData, TotalPlaced, AllocatedProperties);

		// Get write ranges
		TPCGValueRange<FTransform> OutTransforms = OutPointData->GetTransformValueRange(false);
		TPCGValueRange<FVector> OutBoundsMin = OutPointData->GetBoundsMinValueRange(false);
		TPCGValueRange<FVector> OutBoundsMax = OutPointData->GetBoundsMaxValueRange(false);
		TPCGValueRange<int32> OutSeeds = OutPointData->GetSeedValueRange(false);

		// Create output facade for attribute writing
		OutputFacade = MakeShared<PCGExData::FFacade>(OutputIO.ToSharedRef());

		// Create attribute writers
		TSharedPtr<PCGExData::TBuffer<int64>> EntryHashWriter = OutputFacade->GetWritable<int64>(PCGExCollections::Labels::Tag_EntryIdx, 0, true, PCGExData::EBufferInit::Inherit);
		TSharedPtr<PCGExData::TBuffer<FName>> ModuleNameWriter;
		TSharedPtr<PCGExData::TBuffer<int32>> DepthWriter;
		TSharedPtr<PCGExData::TBuffer<int32>> SeedIndexWriter;

		if (Settings->bOutputModuleName)
		{
			ModuleNameWriter = OutputFacade->GetWritable<FName>(Settings->ModuleNameAttributeName, NAME_None, true, PCGExData::EBufferInit::Inherit);
		}

		if (Settings->bOutputDepth)
		{
			DepthWriter = OutputFacade->GetWritable<int32>(Settings->DepthAttributeName, 0, true, PCGExData::EBufferInit::Inherit);
		}

		if (Settings->bOutputSeedIndex)
		{
			SeedIndexWriter = OutputFacade->GetWritable<int32>(Settings->SeedIndexAttributeName, 0, true, PCGExData::EBufferInit::Inherit);
		}

		// Initialize property writer
		TSharedPtr<FPCGExValencyPropertyWriter> PropertyWriter;
		if (Context->BondingRules && Context->BondingRules->IsCompiled())
		{
			PropertyWriter = MakeShared<FPCGExValencyPropertyWriter>();
			PropertyWriter->Initialize(
				Context->BondingRules,
				CompiledRules,
				OutputFacade.ToSharedRef(),
				Settings->PropertiesOutput);
		}

		// Fitting handler
		FPCGExFittingDetailsHandler FittingHandler;
		FittingHandler.ScaleToFit = Settings->ScaleToFit;
		FittingHandler.Justification = Settings->Justification;
		FittingHandler.Init(Context, OutputFacade.ToSharedRef());

		// Write output points
		for (int32 PlacedIdx = 0; PlacedIdx < TotalPlaced; ++PlacedIdx)
		{
			const FPCGExPlacedModule& Placed = PlacedModules[PlacedIdx];
			const int32 ModuleIdx = Placed.ModuleIndex;

			// Transform
			FTransform& OutTransform = OutTransforms[PlacedIdx];
			OutTransform = Placed.WorldTransform;

			// Seed for deterministic downstream use
			OutSeeds[PlacedIdx] = HashCombine(Settings->Seed, PlacedIdx);

			// Apply local transform if enabled
			if (Settings->bApplyLocalTransforms && CompiledRules->ModuleHasLocalTransform[ModuleIdx])
			{
				const FTransform LocalTransform = CompiledRules->GetModuleLocalTransform(ModuleIdx, OutSeeds[PlacedIdx]);
				OutTransform = LocalTransform * OutTransform;
			}

			// Write entry hash for collection map
			if (EntryHashWriter && Context->PickPacker)
			{
				const EPCGExValencyAssetType AssetType = CompiledRules->ModuleAssetTypes[ModuleIdx];
				const UPCGExAssetCollection* Collection = nullptr;
				FPCGExEntryAccessResult Result{};
				int32 EntryIndex = -1;
				int16 SecondaryIndex = -1;

				if (AssetType == EPCGExValencyAssetType::Mesh)
				{
					Collection = Context->MeshCollection;
					EntryIndex = Context->BondingRules->GetMeshEntryIndex(ModuleIdx);
					if (Collection)
					{
						Result = Collection->GetEntryRaw(EntryIndex);
						if (Result.IsValid())
						{
							if (const PCGExAssetCollection::FMicroCache* MicroCache = Result.Entry->MicroCache.Get())
							{
								SecondaryIndex = MicroCache->GetPickRandomWeighted(OutSeeds[PlacedIdx]);
							}
						}
					}
				}
				else if (AssetType == EPCGExValencyAssetType::Actor)
				{
					Collection = Context->ActorCollection;
					EntryIndex = Context->BondingRules->GetActorEntryIndex(ModuleIdx);
					if (Collection)
					{
						Result = Collection->GetEntryRaw(EntryIndex);
					}
				}

				if (Collection && Result.IsValid())
				{
					const uint64 Hash = Context->PickPacker->GetPickIdx(Collection, EntryIndex, SecondaryIndex);
					EntryHashWriter->SetValue(PlacedIdx, static_cast<int64>(Hash));

					// Apply fitting
					FBox OutBounds = Result.Entry->Staging.Bounds;
					FVector Translation = FVector::ZeroVector;
					FittingHandler.ComputeTransform(PlacedIdx, OutTransform, OutBounds, Translation);
					OutBoundsMin[PlacedIdx] = OutBounds.Min;
					OutBoundsMax[PlacedIdx] = OutBounds.Max;
				}
			}

			// Write module name
			if (ModuleNameWriter)
			{
				ModuleNameWriter->SetValue(PlacedIdx, CompiledRules->ModuleNames[ModuleIdx]);
			}

			// Write depth
			if (DepthWriter)
			{
				DepthWriter->SetValue(PlacedIdx, Placed.Depth);
			}

			// Write seed index
			if (SeedIndexWriter)
			{
				SeedIndexWriter->SetValue(PlacedIdx, Placed.SeedIndex);
			}

			// Write properties
			if (PropertyWriter)
			{
				PropertyWriter->WriteModuleProperties(PlacedIdx, ModuleIdx);
			}
		}
	}

	void FProcessor::CompleteWork()
	{
		if (OutputFacade)
		{
			OutputFacade->WriteFastest(TaskManager);
		}
	}

	void FProcessor::Output()
	{
		if (OutputIO)
		{
			OutputIO->StageOutput(Context);
		}
	}
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
