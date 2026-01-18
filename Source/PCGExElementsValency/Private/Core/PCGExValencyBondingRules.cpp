// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExValencyBondingRules.h"

#include "Collections/PCGExMeshCollection.h"
#include "Collections/PCGExActorCollection.h"
#include "Engine/Blueprint.h"

void FPCGExValencyBondingRulesCompiled::BuildCandidateLookup()
{
	MaskToCandidates.Empty();

	// For each unique orbital mask combination, collect candidate modules
	// This is most useful for single-layer bonding rules
	if (Layers.Num() == 1)
	{
		for (int32 ModuleIndex = 0; ModuleIndex < ModuleCount; ++ModuleIndex)
		{
			const int64 Mask = ModuleOrbitalMasks[ModuleIndex];
			TArray<int32>& Candidates = MaskToCandidates.FindOrAdd(Mask);
			Candidates.Add(ModuleIndex);
		}
	}
	// For multi-layer, we could build more complex lookup but it's less beneficial
	// Runtime will iterate modules and check masks directly
}

bool UPCGExValencyBondingRules::Compile()
{
	// Validate orbital sets (TObjectPtr ensures they're already loaded with this asset)
	for (int32 i = 0; i < OrbitalSets.Num(); ++i)
	{
		if (!OrbitalSets[i])
		{
			UE_LOG(LogTemp, Error, TEXT("Valency Bonding Rules: Orbital set at index %d is null"), i);
			return false;
		}

		// Validate orbital set
		TArray<FText> ValidationErrors;
		if (!OrbitalSets[i]->Validate(ValidationErrors))
		{
			for (const FText& Error : ValidationErrors)
			{
				UE_LOG(LogTemp, Error, TEXT("Valency Bonding Rules: %s"), *Error.ToString());
			}
			return false;
		}

		// Check for more than 64 orbitals
		if (OrbitalSets[i]->Num() > 64)
		{
			UE_LOG(LogTemp, Error, TEXT("Valency Bonding Rules: Layer '%s' has more than 64 orbitals"), *OrbitalSets[i]->LayerName.ToString());
			return false;
		}
	}

	// Create compiled data (using MakeShared - safe to call from any thread)
	if (!CompiledData)
	{
		CompiledData = MakeShared<FPCGExValencyBondingRulesCompiled>();
	}

	const int32 LayerCount = OrbitalSets.Num();

	CompiledData->ModuleCount = Modules.Num();
	CompiledData->Layers.SetNum(LayerCount);

	// Initialize parallel arrays
	CompiledData->ModuleWeights.SetNum(Modules.Num());
	CompiledData->ModuleMinSpawns.SetNum(Modules.Num());
	CompiledData->ModuleMaxSpawns.SetNum(Modules.Num());
	CompiledData->ModuleAssets.SetNum(Modules.Num());
	CompiledData->ModuleAssetTypes.SetNum(Modules.Num());
	CompiledData->ModuleLocalTransforms.SetNum(Modules.Num());
	CompiledData->ModuleHasLocalTransform.SetNum(Modules.Num());
	CompiledData->ModuleOrbitalMasks.SetNum(Modules.Num() * LayerCount);
	CompiledData->ModuleBoundaryMasks.SetNum(Modules.Num() * LayerCount);

	// Populate module data
	for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ++ModuleIndex)
	{
		const FPCGExValencyModuleDefinition& Module = Modules[ModuleIndex];

		CompiledData->ModuleWeights[ModuleIndex] = Module.Settings.Weight;
		CompiledData->ModuleMinSpawns[ModuleIndex] = Module.Settings.MinSpawns;
		CompiledData->ModuleMaxSpawns[ModuleIndex] = Module.Settings.MaxSpawns;
		CompiledData->ModuleAssets[ModuleIndex] = Module.Asset;
		CompiledData->ModuleAssetTypes[ModuleIndex] = Module.AssetType;
		CompiledData->ModuleLocalTransforms[ModuleIndex] = Module.LocalTransform;
		CompiledData->ModuleHasLocalTransform[ModuleIndex] = Module.bHasLocalTransform;

		// Orbital masks per layer
		for (int32 LayerIndex = 0; LayerIndex < LayerCount; ++LayerIndex)
		{
			const FName& LayerName = OrbitalSets[LayerIndex]->LayerName;
			const int32 MaskIndex = ModuleIndex * LayerCount + LayerIndex;

			if (const FPCGExValencyModuleLayerConfig* LayerConfig = Module.Layers.Find(LayerName))
			{
				CompiledData->ModuleOrbitalMasks[MaskIndex] = LayerConfig->OrbitalMask;
				CompiledData->ModuleBoundaryMasks[MaskIndex] = LayerConfig->BoundaryOrbitalMask;
			}
			else
			{
				CompiledData->ModuleOrbitalMasks[MaskIndex] = 0;
				CompiledData->ModuleBoundaryMasks[MaskIndex] = 0;
			}
		}
	}

	// Compile each layer's neighbor data
	for (int32 LayerIndex = 0; LayerIndex < LayerCount; ++LayerIndex)
	{
		const UPCGExValencyOrbitalSet* OrbitalSet = OrbitalSets[LayerIndex];
		FPCGExValencyLayerCompiled& CompiledLayer = CompiledData->Layers[LayerIndex];

		CompiledLayer.LayerName = OrbitalSet->LayerName;
		CompiledLayer.OrbitalCount = OrbitalSet->Num();

		// Prepare header array (ModuleCount * OrbitalCount entries)
		const int32 HeaderCount = Modules.Num() * OrbitalSet->Num();
		CompiledLayer.NeighborHeaders.SetNum(HeaderCount);
		CompiledLayer.AllNeighbors.Empty();

		// Flatten neighbor data
		for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ++ModuleIndex)
		{
			const FPCGExValencyModuleDefinition& Module = Modules[ModuleIndex];
			const FPCGExValencyModuleLayerConfig* LayerConfig = Module.Layers.Find(OrbitalSet->LayerName);

			for (int32 OrbitalIndex = 0; OrbitalIndex < OrbitalSet->Num(); ++OrbitalIndex)
			{
				const int32 HeaderIndex = ModuleIndex * OrbitalSet->Num() + OrbitalIndex;
				const FName OrbitalName = OrbitalSet->Orbitals[OrbitalIndex].GetOrbitalName();

				int32 NeighborStart = CompiledLayer.AllNeighbors.Num();
				int32 NeighborCount = 0;

				if (LayerConfig)
				{
					if (const FPCGExValencyNeighborIndices* Neighbors = LayerConfig->OrbitalNeighbors.Find(OrbitalName))
					{
						NeighborCount = Neighbors->Num();
						CompiledLayer.AllNeighbors.Append(Neighbors->Indices);
					}
				}

				CompiledLayer.NeighborHeaders[HeaderIndex] = FIntPoint(NeighborStart, NeighborCount);
			}
		}
	}

	// Build fast lookup
	CompiledData->BuildCandidateLookup();

	UE_LOG(LogTemp, Log, TEXT("Valency Bonding Rules compiled: %d modules, %d layers"), Modules.Num(), LayerCount);
	return true;
}

void UPCGExValencyBondingRules::PostLoad()
{
	Super::PostLoad();

	// Recompile on load since CompiledData is not serialized
	if (Modules.Num() > 0 && OrbitalSets.Num() > 0)
	{
		Compile();
	}
}

void UPCGExValencyBondingRules::RebuildGeneratedCollections()
{
	// Initialize module-to-entry mappings
	ModuleToMeshEntryIndex.SetNum(Modules.Num());
	ModuleToActorEntryIndex.SetNum(Modules.Num());
	for (int32 i = 0; i < Modules.Num(); ++i)
	{
		ModuleToMeshEntryIndex[i] = -1;
		ModuleToActorEntryIndex[i] = -1;
	}

	// Count modules by type
	int32 MeshCount = 0;
	int32 ActorCount = 0;

	for (const FPCGExValencyModuleDefinition& Module : Modules)
	{
		if (Module.AssetType == EPCGExValencyAssetType::Mesh)
		{
			MeshCount++;
		}
		else if (Module.AssetType == EPCGExValencyAssetType::Actor)
		{
			ActorCount++;
		}
	}

	// Create or clear mesh collection
	if (MeshCount > 0)
	{
		if (!GeneratedMeshCollection)
		{
			const FName CollectionName = *FString::Printf(TEXT("%s_Meshes"), *GetName());
			GeneratedMeshCollection = NewObject<UPCGExMeshCollection>(this, CollectionName, RF_Transactional);
		}
		GeneratedMeshCollection->Entries.Empty();
		GeneratedMeshCollection->Entries.Reserve(MeshCount);

		int32 EntryIndex = 0;
		for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ++ModuleIndex)
		{
			const FPCGExValencyModuleDefinition& Module = Modules[ModuleIndex];
			if (Module.AssetType == EPCGExValencyAssetType::Mesh)
			{
				FPCGExMeshCollectionEntry& Entry = GeneratedMeshCollection->Entries.AddDefaulted_GetRef();
				Entry.StaticMesh = TSoftObjectPtr<UStaticMesh>(Module.Asset.ToSoftObjectPath());
				Entry.Weight = FMath::Max(1, FMath::RoundToInt32(Module.Settings.Weight));

				// Populate material variants if discovered
				const FSoftObjectPath MeshPath = Module.Asset.ToSoftObjectPath();
				if (const TArray<FPCGExValencyMaterialVariant>* Variants = DiscoveredMaterialVariants.Find(MeshPath))
				{
					if (Variants->Num() > 0)
					{
						// Determine mode: Single if all variants override same single slot
						bool bCanUseSingleMode = true;
						int32 CommonSlotIndex = -1;

						for (const FPCGExValencyMaterialVariant& Variant : *Variants)
						{
							if (Variant.Overrides.Num() != 1)
							{
								bCanUseSingleMode = false;
								break;
							}
							if (CommonSlotIndex < 0)
							{
								CommonSlotIndex = Variant.Overrides[0].SlotIndex;
							}
							else if (Variant.Overrides[0].SlotIndex != CommonSlotIndex)
							{
								bCanUseSingleMode = false;
								break;
							}
						}

						if (bCanUseSingleMode && CommonSlotIndex >= 0)
						{
							// Single slot mode
							Entry.MaterialVariants = EPCGExMaterialVariantsMode::Single;
							Entry.SlotIndex = CommonSlotIndex;
							Entry.MaterialOverrideVariants.Empty();
							Entry.MaterialOverrideVariants.Reserve(Variants->Num());

							for (const FPCGExValencyMaterialVariant& Variant : *Variants)
							{
								FPCGExMaterialOverrideSingleEntry& SingleEntry = Entry.MaterialOverrideVariants.AddDefaulted_GetRef();
								SingleEntry.Weight = Variant.DiscoveryCount;
								SingleEntry.Material = Variant.Overrides[0].Material;
							}
						}
						else
						{
							// Multi slot mode
							Entry.MaterialVariants = EPCGExMaterialVariantsMode::Multi;
							Entry.MaterialOverrideVariantsList.Empty();
							Entry.MaterialOverrideVariantsList.Reserve(Variants->Num());

							for (const FPCGExValencyMaterialVariant& Variant : *Variants)
							{
								FPCGExMaterialOverrideCollection& MultiEntry = Entry.MaterialOverrideVariantsList.AddDefaulted_GetRef();
								MultiEntry.Weight = Variant.DiscoveryCount;
								MultiEntry.Overrides.Reserve(Variant.Overrides.Num());

								for (const FPCGExValencyMaterialOverride& Override : Variant.Overrides)
								{
									FPCGExMaterialOverrideEntry& OverrideEntry = MultiEntry.Overrides.AddDefaulted_GetRef();
									OverrideEntry.SlotIndex = Override.SlotIndex;
									OverrideEntry.Material = Override.Material;
								}
							}
						}
					}
				}

				// Store mapping
				ModuleToMeshEntryIndex[ModuleIndex] = EntryIndex;
				++EntryIndex;
			}
		}

		// Rebuild internal indices
		GeneratedMeshCollection->RebuildStagingData(true);
	}
	else
	{
		GeneratedMeshCollection = nullptr;
	}

	// Create or clear actor collection
	if (ActorCount > 0)
	{
		if (!GeneratedActorCollection)
		{
			const FName CollectionName = *FString::Printf(TEXT("%s_Actors"), *GetName());
			GeneratedActorCollection = NewObject<UPCGExActorCollection>(this, CollectionName, RF_Transactional);
		}
		GeneratedActorCollection->Entries.Empty();
		GeneratedActorCollection->Entries.Reserve(ActorCount);

		int32 EntryIndex = 0;
		for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ++ModuleIndex)
		{
			const FPCGExValencyModuleDefinition& Module = Modules[ModuleIndex];
			if (Module.AssetType == EPCGExValencyAssetType::Actor)
			{
				// Actor modules store a Blueprint, we need to get the GeneratedClass from it
				if (UObject* LoadedAsset = Module.Asset.LoadSynchronous())
				{
					if (const UBlueprint* Blueprint = Cast<UBlueprint>(LoadedAsset))
					{
						if (Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass()))
						{
							FPCGExActorCollectionEntry& Entry = GeneratedActorCollection->Entries.AddDefaulted_GetRef();
							Entry.Actor = TSoftClassPtr<AActor>(Blueprint->GeneratedClass.Get());
							Entry.Weight = FMath::Max(1, FMath::RoundToInt32(Module.Settings.Weight));

							// Store mapping
							ModuleToActorEntryIndex[ModuleIndex] = EntryIndex;
							++EntryIndex;
						}
					}
				}
			}
		}

		// Rebuild internal indices if we have entries
		if (GeneratedActorCollection->Entries.Num() > 0)
		{
			GeneratedActorCollection->RebuildStagingData(true);
		}
		else
		{
			GeneratedActorCollection = nullptr;
		}
	}
	else
	{
		GeneratedActorCollection = nullptr;
	}

	// Mark as dirty for save
	Modify();
}

#if WITH_EDITOR
void UPCGExValencyBondingRules::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Auto-compile when properties change in editor
	// This could be made optional via a flag if compilation is expensive
	// Compile();
}
#endif
