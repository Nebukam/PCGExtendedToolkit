// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExValencyBondingRules.h"

#include "Collections/PCGExMeshCollection.h"
#include "Collections/PCGExActorCollection.h"
#include "Engine/Blueprint.h"
#include "Core/PCGExValencyLog.h"

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
	VALENCY_LOG_SECTION(Compilation, "BONDING RULES COMPILATION START");
	PCGEX_VALENCY_INFO(Compilation, "Asset: %s", *GetName());
	PCGEX_VALENCY_INFO(Compilation, "Module count: %d, OrbitalSet count: %d", Modules.Num(), OrbitalSets.Num());

	// Validate orbital sets (TObjectPtr ensures they're already loaded with this asset)
	for (int32 i = 0; i < OrbitalSets.Num(); ++i)
	{
		if (!OrbitalSets[i])
		{
			PCGEX_VALENCY_ERROR(Compilation, "Orbital set at index %d is null", i);
			return false;
		}

		PCGEX_VALENCY_INFO(Compilation, "  OrbitalSet[%d]: '%s' with %d orbitals", i, *OrbitalSets[i]->LayerName.ToString(), OrbitalSets[i]->Num());

		// Validate orbital set
		TArray<FText> ValidationErrors;
		if (!OrbitalSets[i]->Validate(ValidationErrors))
		{
			for (const FText& Error : ValidationErrors)
			{
				PCGEX_VALENCY_ERROR(Compilation, "%s", *Error.ToString());
			}
			return false;
		}

		// Check for more than 64 orbitals
		if (OrbitalSets[i]->Num() > 64)
		{
			PCGEX_VALENCY_ERROR(Compilation, "Layer '%s' has more than 64 orbitals", *OrbitalSets[i]->LayerName.ToString());
			return false;
		}
	}

	// Reset compiled data for fresh compilation
	CompiledData = FPCGExValencyBondingRulesCompiled();

	const int32 LayerCount = OrbitalSets.Num();

	CompiledData.ModuleCount = Modules.Num();
	CompiledData.Layers.SetNum(LayerCount);

	// Initialize parallel arrays
	CompiledData.ModuleWeights.SetNum(Modules.Num());
	CompiledData.ModuleMinSpawns.SetNum(Modules.Num());
	CompiledData.ModuleMaxSpawns.SetNum(Modules.Num());
	CompiledData.ModuleAssets.SetNum(Modules.Num());
	CompiledData.ModuleAssetTypes.SetNum(Modules.Num());
	CompiledData.ModuleNames.SetNum(Modules.Num());
	CompiledData.ModuleLocalTransformHeaders.SetNum(Modules.Num());
	CompiledData.ModuleHasLocalTransform.SetNum(Modules.Num());
	CompiledData.ModuleOrbitalMasks.SetNum(Modules.Num() * LayerCount);
	CompiledData.ModuleBoundaryMasks.SetNum(Modules.Num() * LayerCount);
	CompiledData.ModuleWildcardMasks.SetNum(Modules.Num() * LayerCount);
	CompiledData.AllLocalTransforms.Empty();
	CompiledData.ModulePropertyHeaders.SetNum(Modules.Num());
	CompiledData.AllModuleProperties.Empty();
	CompiledData.ModuleTags.SetNum(Modules.Num());

	// Populate module data
	VALENCY_LOG_SUBSECTION(Compilation, "Compiling Module Data");
	for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ++ModuleIndex)
	{
		const FPCGExValencyModuleDefinition& Module = Modules[ModuleIndex];

		CompiledData.ModuleWeights[ModuleIndex] = Module.Settings.Weight;
		CompiledData.ModuleMinSpawns[ModuleIndex] = Module.Settings.MinSpawns;
		CompiledData.ModuleMaxSpawns[ModuleIndex] = Module.Settings.MaxSpawns;
		CompiledData.ModuleAssets[ModuleIndex] = Module.Asset;
		CompiledData.ModuleAssetTypes[ModuleIndex] = Module.AssetType;
		CompiledData.ModuleNames[ModuleIndex] = Module.ModuleName;
		CompiledData.ModuleHasLocalTransform[ModuleIndex] = Module.bHasLocalTransform;

		// Populate transform header and flattened transforms
		const int32 TransformStartIndex = CompiledData.AllLocalTransforms.Num();
		const int32 TransformCount = Module.LocalTransforms.Num();
		CompiledData.ModuleLocalTransformHeaders[ModuleIndex] = FIntPoint(TransformStartIndex, TransformCount);
		CompiledData.AllLocalTransforms.Append(Module.LocalTransforms);

		// Populate property header and flattened properties
		const int32 PropertyStartIndex = CompiledData.AllModuleProperties.Num();
		const int32 PropertyCount = Module.Properties.Num();
		CompiledData.ModulePropertyHeaders[ModuleIndex] = FIntPoint(PropertyStartIndex, PropertyCount);
		CompiledData.AllModuleProperties.Append(Module.Properties);

		// Copy module tags
		CompiledData.ModuleTags[ModuleIndex] = FPCGExValencyModuleTags(Module.Tags);

		PCGEX_VALENCY_VERBOSE(Compilation, "  Module[%d]: Asset='%s', Weight=%.2f, Type=%d, Properties=%d, Tags=%d",
			ModuleIndex,
			*Module.Asset.GetAssetName(),
			Module.Settings.Weight,
			static_cast<int32>(Module.AssetType),
			PropertyCount,
			Module.Tags.Num());

		// Orbital masks per layer
		for (int32 LayerIndex = 0; LayerIndex < LayerCount; ++LayerIndex)
		{
			const FName& LayerName = OrbitalSets[LayerIndex]->LayerName;
			const int32 MaskIndex = ModuleIndex * LayerCount + LayerIndex;

			if (const FPCGExValencyModuleLayerConfig* LayerConfig = Module.Layers.Find(LayerName))
			{
				CompiledData.ModuleOrbitalMasks[MaskIndex] = LayerConfig->OrbitalMask;
				CompiledData.ModuleBoundaryMasks[MaskIndex] = LayerConfig->BoundaryOrbitalMask;
				CompiledData.ModuleWildcardMasks[MaskIndex] = LayerConfig->WildcardOrbitalMask;

				// Log orbital mask as binary for easier reading
				FString OrbitalBits;
				FString BoundaryBits;
				FString WildcardBits;
				for (int32 Bit = 0; Bit < OrbitalSets[LayerIndex]->Num(); ++Bit)
				{
					OrbitalBits += (LayerConfig->OrbitalMask & (1LL << Bit)) ? TEXT("1") : TEXT("0");
					BoundaryBits += (LayerConfig->BoundaryOrbitalMask & (1LL << Bit)) ? TEXT("1") : TEXT("0");
					WildcardBits += (LayerConfig->WildcardOrbitalMask & (1LL << Bit)) ? TEXT("1") : TEXT("0");
				}
				PCGEX_VALENCY_VERBOSE(Compilation, "    Layer[%d] '%s': OrbitalMask=%s, BoundaryMask=%s, WildcardMask=%s",
					LayerIndex, *LayerName.ToString(),
					*OrbitalBits, *BoundaryBits, *WildcardBits);

				// Log neighbor info
				for (const auto& NeighborPair : LayerConfig->OrbitalNeighbors)
				{
					PCGEX_VALENCY_VERBOSE(Compilation, "      Orbital '%s' neighbors: [%s]",
						*NeighborPair.Key.ToString(),
						*FString::JoinBy(NeighborPair.Value.Indices, TEXT(", "), [](int32 Idx) { return FString::FromInt(Idx); }));
				}
			}
			else
			{
				CompiledData.ModuleOrbitalMasks[MaskIndex] = 0;
				CompiledData.ModuleBoundaryMasks[MaskIndex] = 0;
				CompiledData.ModuleWildcardMasks[MaskIndex] = 0;
				PCGEX_VALENCY_VERBOSE(Compilation, "    Layer[%d] '%s': NO CONFIG (masks=0)", LayerIndex, *LayerName.ToString());
			}
		}
	}

	// Compile each layer's neighbor data
	for (int32 LayerIndex = 0; LayerIndex < LayerCount; ++LayerIndex)
	{
		const UPCGExValencyOrbitalSet* OrbitalSet = OrbitalSets[LayerIndex];
		FPCGExValencyLayerCompiled& CompiledLayer = CompiledData.Layers[LayerIndex];

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
	CompiledData.BuildCandidateLookup();

	// Build module property registry
	VALENCY_LOG_SUBSECTION(Compilation, "Building Property Registries");
	{
		TMap<FName, FPCGExPropertyRegistryEntry> ModulePropertiesMap;

		// Scan all module properties
		for (const FInstancedStruct& PropStruct : CompiledData.AllModuleProperties)
		{
			if (const FPCGExPropertyCompiled* Prop = PropStruct.GetPtr<FPCGExPropertyCompiled>())
			{
				if (!Prop->PropertyName.IsNone() && !ModulePropertiesMap.Contains(Prop->PropertyName))
				{
					ModulePropertiesMap.Add(Prop->PropertyName, Prop->ToRegistryEntry());
				}
			}
		}

		// Convert to array and sort by name for consistent ordering
		ModulePropertiesMap.GenerateValueArray(CompiledData.ModulePropertyRegistry);
		CompiledData.ModulePropertyRegistry.Sort([](const FPCGExPropertyRegistryEntry& A, const FPCGExPropertyRegistryEntry& B)
		{
			return A.PropertyName.LexicalLess(B.PropertyName);
		});

		PCGEX_VALENCY_INFO(Compilation, "Module property registry: %d unique properties", CompiledData.ModulePropertyRegistry.Num());
		for (const FPCGExPropertyRegistryEntry& Entry : CompiledData.ModulePropertyRegistry)
		{
			PCGEX_VALENCY_VERBOSE(Compilation, "  - %s (%s, output=%s)",
				*Entry.PropertyName.ToString(),
				*Entry.TypeName.ToString(),
				Entry.bSupportsOutput ? TEXT("yes") : TEXT("no"));
		}
	}

	// Build pattern property registry
	{
		TMap<FName, FPCGExPropertyRegistryEntry> PatternPropertiesMap;

		// Scan all pattern entry properties
		for (const FPCGExValencyPatternCompiled& Pattern : Patterns.Patterns)
		{
			for (const FPCGExValencyPatternEntryCompiled& Entry : Pattern.Entries)
			{
				for (const FInstancedStruct& PropStruct : Entry.Properties)
				{
					if (const FPCGExPropertyCompiled* Prop = PropStruct.GetPtr<FPCGExPropertyCompiled>())
					{
						if (!Prop->PropertyName.IsNone() && !PatternPropertiesMap.Contains(Prop->PropertyName))
						{
							PatternPropertiesMap.Add(Prop->PropertyName, Prop->ToRegistryEntry());
						}
					}
				}
			}
		}

		// Convert to array and sort
		PatternPropertiesMap.GenerateValueArray(CompiledData.PatternPropertyRegistry);
		CompiledData.PatternPropertyRegistry.Sort([](const FPCGExPropertyRegistryEntry& A, const FPCGExPropertyRegistryEntry& B)
		{
			return A.PropertyName.LexicalLess(B.PropertyName);
		});

		PCGEX_VALENCY_INFO(Compilation, "Pattern property registry: %d unique properties", CompiledData.PatternPropertyRegistry.Num());
		for (const FPCGExPropertyRegistryEntry& Entry : CompiledData.PatternPropertyRegistry)
		{
			PCGEX_VALENCY_VERBOSE(Compilation, "  - %s (%s, output=%s)",
				*Entry.PropertyName.ToString(),
				*Entry.TypeName.ToString(),
				Entry.bSupportsOutput ? TEXT("yes") : TEXT("no"));
		}
	}

	// Copy patterns (already compiled by builder, stored on this asset)
	CompiledData.CompiledPatterns = Patterns;
	PCGEX_VALENCY_INFO(Compilation, "Patterns: %d total (%d exclusive, %d additive)",
		Patterns.GetPatternCount(),
		Patterns.ExclusivePatternIndices.Num(),
		Patterns.AdditivePatternIndices.Num());

	VALENCY_LOG_SECTION(Compilation, "BONDING RULES COMPILATION COMPLETE");
	PCGEX_VALENCY_INFO(Compilation, "Result: %d modules, %d layers, %d patterns", Modules.Num(), LayerCount, Patterns.GetPatternCount());
	return true;
}

void UPCGExValencyBondingRules::PostLoad()
{
	Super::PostLoad();

	// If already compiled (serialized data), just rebuild the non-serialized lookup table
	if (IsCompiled())
	{
		CompiledData.BuildCandidateLookup();
		return;
	}

	// Otherwise compile if we have the required data
	if (Modules.Num() > 0 && OrbitalSets.Num() > 0)
	{
		Compile();
	}
}

void UPCGExValencyBondingRules::EDITOR_RegisterTrackingKeys(FPCGExContext* Context) const
{
#if WITH_EDITOR
	Context->EDITOR_TrackPath(this);
	if (GeneratedMeshCollection) { GeneratedMeshCollection->EDITOR_RegisterTrackingKeys(Context); }
	if (GeneratedActorCollection) { GeneratedActorCollection->EDITOR_RegisterTrackingKeys(Context); }
#endif
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

				// Populate material variant if this module has one
				// Each module now has its own material variant stored directly, not looked up by mesh path
				if (Module.bHasMaterialVariant && Module.MaterialVariant.Overrides.Num() > 0)
				{
					const FPCGExValencyMaterialVariant& Variant = Module.MaterialVariant;

					// Determine mode based on number of overrides
					if (Variant.Overrides.Num() == 1)
					{
						// Single slot mode
						Entry.MaterialVariants = EPCGExMaterialVariantsMode::Single;
						Entry.SlotIndex = Variant.Overrides[0].SlotIndex;
						Entry.MaterialOverrideVariants.Empty();

						FPCGExMaterialOverrideSingleEntry& SingleEntry = Entry.MaterialOverrideVariants.AddDefaulted_GetRef();
						SingleEntry.Weight = Variant.DiscoveryCount;
						SingleEntry.Material = Variant.Overrides[0].Material;
					}
					else
					{
						// Multi slot mode
						Entry.MaterialVariants = EPCGExMaterialVariantsMode::Multi;
						Entry.MaterialOverrideVariantsList.Empty();

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
