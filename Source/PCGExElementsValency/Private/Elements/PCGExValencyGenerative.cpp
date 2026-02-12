// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExValencyGenerative.h"

#include "PCGParamData.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGExData.h"
#include "Helpers/PCGExCollectionsHelpers.h"
#include "Helpers/PCGExPointArrayDataHelpers.h"
#include "Helpers/PCGExStreamingHelpers.h"
#include "PCGExCollectionsCommon.h"
#include "Collections/PCGExActorCollection.h"
#include "Collections/PCGExMeshCollection.h"
#include "Containers/PCGExManagedObjects.h"
#include "Growth/PCGExValencyGrowthBFS.h"

#define LOCTEXT_NAMESPACE "PCGExValencyGenerative"
#define PCGEX_NAMESPACE ValencyGenerative

#pragma region UPCGExValencyGenerativeSettings

void UPCGExValencyGenerativeSettings::PostInitProperties()
{
	if (!HasAnyFlags(RF_ClassDefaultObject) && IsInGameThread())
	{
		if (!GrowthStrategy) { GrowthStrategy = NewObject<UPCGExValencyGrowthBFSFactory>(this, TEXT("GrowthStrategy")); }
	}
	Super::PostInitProperties();
}

TArray<FPCGPinProperties> UPCGExValencyGenerativeSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGPinConstants::DefaultInputLabel, "Seed points with transforms", Required)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExValencyGenerativeSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGPinConstants::DefaultOutputLabel, "Generated points with module assignments and transforms", Required)
	PCGEX_PIN_PARAMS(PCGExCollections::Labels::OutputCollectionMapLabel, "Collection map for resolving entry hashes", Required)
	return PinProperties;
}

FPCGElementPtr UPCGExValencyGenerativeSettings::CreateElement() const
{
	return MakeShared<FPCGExValencyGenerativeElement>();
}

#pragma endregion

#pragma region FPCGExValencyGenerativeContext

void FPCGExValencyGenerativeContext::RegisterAssetDependencies()
{
	FPCGExContext::RegisterAssetDependencies();

	const UPCGExValencyGenerativeSettings* Settings = GetInputSettings<UPCGExValencyGenerativeSettings>();
	if (!Settings) { return; }

	if (!Settings->BondingRules.IsNull())
	{
		AddAssetDependency(Settings->BondingRules.ToSoftObjectPath());
	}

	if (!Settings->SocketRules.IsNull())
	{
		AddAssetDependency(Settings->SocketRules.ToSoftObjectPath());
	}
}

#pragma endregion

#pragma region FPCGExValencyGenerativeElement

bool FPCGExValencyGenerativeElement::Boot(FPCGExContext* InContext) const
{
	if (!IPCGExElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ValencyGenerative)

	// Validate required settings
	if (Settings->BondingRules.IsNull())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("No Bonding Rules provided."));
		return false;
	}

	if (Settings->SocketRules.IsNull())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("No Socket Rules provided."));
		return false;
	}

	PCGEX_OPERATION_VALIDATE(GrowthStrategy)

	// Load assets
	PCGExHelpers::LoadBlocking_AnyThreadTpl(Settings->BondingRules, InContext);
	PCGExHelpers::LoadBlocking_AnyThreadTpl(Settings->SocketRules, InContext);

	return true;
}

void FPCGExValencyGenerativeElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
	IPCGExElement::PostLoadAssetsDependencies(InContext);

	PCGEX_CONTEXT_AND_SETTINGS(ValencyGenerative)

	Context->BondingRules = Settings->BondingRules.Get();
	Context->SocketRules = Settings->SocketRules.Get();
}

bool FPCGExValencyGenerativeElement::PostBoot(FPCGExContext* InContext) const
{
	if (!IPCGExElement::PostBoot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ValencyGenerative)

	if (!Context->BondingRules)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Failed to load Bonding Rules."));
		return false;
	}

	if (!Context->SocketRules)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Failed to load Socket Rules."));
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

	return true;
}

bool FPCGExValencyGenerativeElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	PCGEX_CONTEXT_AND_SETTINGS(ValencyGenerative)

	PCGEX_ON_INITIAL_EXECUTION
	{
		const FPCGExValencyBondingRulesCompiled* CompiledRules = Context->BondingRules->GetCompiledData();
		if (!CompiledRules || CompiledRules->ModuleCount == 0)
		{
			return Context->CancelExecution(TEXT("No compiled modules in bonding rules."));
		}

		// Get input seed points
		TArray<FPCGTaggedData> SeedInputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
		if (SeedInputs.IsEmpty())
		{
			return Context->CancelExecution(TEXT("No seed points provided."));
		}

		// Compile socket rules
		Context->SocketRules->Compile();

		// Build module local bounds cache from collection staging data
		TArray<FBox> ModuleLocalBounds;
		ModuleLocalBounds.SetNum(CompiledRules->ModuleCount);
		for (int32 i = 0; i < CompiledRules->ModuleCount; ++i)
		{
			ModuleLocalBounds[i] = FBox(ForceInit);
		}

		// Populate bounds from mesh collection entries
		if (Context->MeshCollection)
		{
			for (int32 ModuleIdx = 0; ModuleIdx < CompiledRules->ModuleCount; ++ModuleIdx)
			{
				const int32 EntryIndex = Context->BondingRules->GetMeshEntryIndex(ModuleIdx);
				if (EntryIndex >= 0)
				{
					const FPCGExEntryAccessResult Result = Context->MeshCollection->GetEntryRaw(EntryIndex);
					if (Result.IsValid())
					{
						ModuleLocalBounds[ModuleIdx] = Result.Entry->Staging.Bounds;

						// Apply inflation
						if (!FMath::IsNearlyZero(Settings->BoundsInflation))
						{
							ModuleLocalBounds[ModuleIdx] = ModuleLocalBounds[ModuleIdx].ExpandBy(Settings->BoundsInflation);
						}
					}
				}
			}
		}

		// Populate bounds from actor collection entries
		if (Context->ActorCollection)
		{
			for (int32 ModuleIdx = 0; ModuleIdx < CompiledRules->ModuleCount; ++ModuleIdx)
			{
				const int32 EntryIndex = Context->BondingRules->GetActorEntryIndex(ModuleIdx);
				if (EntryIndex >= 0)
				{
					const FPCGExEntryAccessResult Result = Context->ActorCollection->GetEntryRaw(EntryIndex);
					if (Result.IsValid())
					{
						ModuleLocalBounds[ModuleIdx] = Result.Entry->Staging.Bounds;

						if (!FMath::IsNearlyZero(Settings->BoundsInflation))
						{
							ModuleLocalBounds[ModuleIdx] = ModuleLocalBounds[ModuleIdx].ExpandBy(Settings->BoundsInflation);
						}
					}
				}
			}
		}

		// Create growth operation
		TSharedPtr<FPCGExValencyGrowthOperation> GrowthOp = Context->GrowthFactory->CreateOperation();
		if (!GrowthOp)
		{
			return Context->CancelExecution(TEXT("Failed to create growth operation."));
		}

		// Setup budget
		FPCGExGrowthBudget Budget = Settings->Budget;
		Budget.Reset();

		// Setup bounds tracker
		FPCGExBoundsTracker BoundsTracker;

		// Initialize growth operation
		GrowthOp->Initialize(CompiledRules, Context->SocketRules, BoundsTracker, Budget, Settings->Seed);

		// Copy local bounds to the growth operation
		GrowthOp->ModuleLocalBounds = ModuleLocalBounds;

		// Collect all seed points and resolve to modules
		TArray<FPCGExPlacedModule> PlacedModules;
		FRandomStream SeedRandom(Settings->Seed);
		int32 SeedIdx = 0;

		// Build name-to-module lookup for seed filtering
		TMap<FName, TArray<int32>> NameToModules;
		for (int32 ModuleIdx = 0; ModuleIdx < CompiledRules->ModuleCount; ++ModuleIdx)
		{
			const FName& ModuleName = CompiledRules->ModuleNames[ModuleIdx];
			if (!ModuleName.IsNone())
			{
				NameToModules.FindOrAdd(ModuleName).Add(ModuleIdx);
			}
		}

		for (const FPCGTaggedData& TaggedData : SeedInputs)
		{
			const UPCGBasePointData* PointData = Cast<UPCGBasePointData>(TaggedData.Data);
			if (!PointData) { continue; }

			const int32 NumPoints = PointData->GetNum();
			if (NumPoints == 0) { continue; }

			TConstPCGValueRange<FTransform> SeedTransforms = PointData->GetConstTransformValueRange();

			// Optional seed module name attribute
			const FPCGMetadataAttribute<FName>* NameAttr = nullptr;
			if (!Settings->SeedModuleNameAttribute.IsNone())
			{
				NameAttr = PointData->ConstMetadata()->GetConstTypedAttribute<FName>(Settings->SeedModuleNameAttribute);
			}

			for (int32 PointIdx = 0; PointIdx < NumPoints; ++PointIdx)
			{
				// Determine candidate modules for this seed
				TArray<int32> CandidateModules;

				if (NameAttr)
				{
					// Filter by module name attribute
					const FName RequestedName = NameAttr->GetValueFromItemKey(static_cast<int64>(PointIdx));
					if (!RequestedName.IsNone())
					{
						if (const TArray<int32>* Matching = NameToModules.Find(RequestedName))
						{
							CandidateModules = *Matching;
						}
					}
				}

				// If no filtering or no match, use all modules that have sockets
				if (CandidateModules.IsEmpty())
				{
					for (int32 ModuleIdx = 0; ModuleIdx < CompiledRules->ModuleCount; ++ModuleIdx)
					{
						if (CompiledRules->GetModuleSocketCount(ModuleIdx) > 0)
						{
							CandidateModules.Add(ModuleIdx);
						}
					}
				}

				if (CandidateModules.IsEmpty()) { continue; }

				// Weighted random selection
				float TotalWeight = 0.0f;
				for (int32 ModuleIdx : CandidateModules)
				{
					TotalWeight += CompiledRules->ModuleWeights[ModuleIdx];
				}

				int32 SelectedModule = CandidateModules[0];
				if (TotalWeight > 0.0f)
				{
					float Pick = SeedRandom.FRand() * TotalWeight;
					for (int32 ModuleIdx : CandidateModules)
					{
						Pick -= CompiledRules->ModuleWeights[ModuleIdx];
						if (Pick <= 0.0f)
						{
							SelectedModule = ModuleIdx;
							break;
						}
					}
				}

				// Create seed placement
				FPCGExPlacedModule SeedModule;
				SeedModule.ModuleIndex = SelectedModule;
				SeedModule.WorldTransform = SeedTransforms[PointIdx];
				SeedModule.WorldBounds = GrowthOp->ComputeWorldBounds(SelectedModule, SeedModule.WorldTransform);
				SeedModule.ParentIndex = -1;
				SeedModule.ParentSocketIndex = -1;
				SeedModule.ChildSocketIndex = -1;
				SeedModule.Depth = 0;
				SeedModule.SeedIndex = SeedIdx;
				SeedModule.CumulativeWeight = CompiledRules->ModuleWeights[SelectedModule];

				PlacedModules.Add(SeedModule);
				Budget.CurrentTotal++;

				// Track bounds
				if (SeedModule.WorldBounds.IsValid)
				{
					BoundsTracker.Add(SeedModule.WorldBounds);
				}

				SeedIdx++;
			}
		}

		if (PlacedModules.IsEmpty())
		{
			return Context->CancelExecution(TEXT("No seed modules could be resolved."));
		}

		// Run the growth
		GrowthOp->Grow(PlacedModules);

		// Create output point data
		const TSharedPtr<PCGExData::FPointIO> OutputIO = PCGExData::NewPointIO(Context, PCGPinConstants::DefaultOutputLabel);
		UPCGBasePointData* OutPointData = OutputIO->GetOut();

		const int32 TotalPlaced = PlacedModules.Num();

		// Allocate points with transform + bounds
		EPCGPointNativeProperties AllocatedProperties = EPCGPointNativeProperties::Transform | EPCGPointNativeProperties::BoundsMin | EPCGPointNativeProperties::BoundsMax | EPCGPointNativeProperties::Seed;
		PCGExPointArrayDataHelpers::SetNumPointsAllocated(OutPointData, TotalPlaced, AllocatedProperties);

		// Get write ranges
		TPCGValueRange<FTransform> OutTransforms = OutPointData->GetTransformValueRange(false);
		TPCGValueRange<FVector> OutBoundsMin = OutPointData->GetBoundsMinValueRange(false);
		TPCGValueRange<FVector> OutBoundsMax = OutPointData->GetBoundsMaxValueRange(false);
		TPCGValueRange<int32> OutSeeds = OutPointData->GetSeedValueRange(false);

		// Create output facade for attribute writing
		const TSharedRef<PCGExData::FFacade> OutputFacade = MakeShared<PCGExData::FFacade>(OutputIO.ToSharedRef());

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
				OutputFacade,
				Settings->PropertiesOutput);
		}

		// Fitting handler
		FPCGExFittingDetailsHandler FittingHandler;
		FittingHandler.ScaleToFit = Settings->ScaleToFit;
		FittingHandler.Justification = Settings->Justification;
		FittingHandler.Init(Context, OutputFacade);

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

		// Flush attribute writes
		OutputFacade->Flush();

		// Stage output points
		OutputIO->StageOutput(Context);

		// Output collection map
		UPCGParamData* ParamData = Context->ManagedObjects->New<UPCGParamData>();
		Context->PickPacker->PackToDataset(ParamData);
		Context->StageOutput(ParamData, PCGExCollections::Labels::OutputCollectionMapLabel, PCGExData::EStaging::None);
	}

	return Context->TryComplete();
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
