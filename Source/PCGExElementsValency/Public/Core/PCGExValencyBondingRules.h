// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PCGExValencyCommon.h"
#include "PCGExValencyOrbitalSet.h"
#include "PCGExValencyPattern.h"

#include "PCGExValencyBondingRules.generated.h"

class UPCGExMeshCollection;
class UPCGExActorCollection;

/**
 * Compiled layer data optimized for runtime performance.
 * Uses flattened arrays for cache efficiency (similar to Unity Chemistry pattern).
 */
USTRUCT()
struct PCGEXELEMENTSVALENCY_API FPCGExValencyLayerCompiled
{
	GENERATED_BODY()

	/** Layer name */
	UPROPERTY()
	FName LayerName;

	/** Number of orbitals in this layer */
	UPROPERTY()
	int32 OrbitalCount = 0;

	/**
	 * Per-module, per-orbital neighbor data.
	 * Index = ModuleIndex * OrbitalCount + OrbitalIndex
	 * Value.X = Start index in AllNeighbors
	 * Value.Y = Count of valid neighbors
	 */
	UPROPERTY()
	TArray<FIntPoint> NeighborHeaders;

	/** Flattened array of all valid neighbor module indices */
	UPROPERTY()
	TArray<int32> AllNeighbors;

	/** Check if a module's orbital accepts a specific neighbor */
	bool OrbitalAcceptsNeighbor(int32 ModuleIndex, int32 OrbitalIndex, int32 NeighborModuleIndex) const
	{
		const int32 HeaderIndex = ModuleIndex * OrbitalCount + OrbitalIndex;
		if (!NeighborHeaders.IsValidIndex(HeaderIndex))
		{
			return false;
		}

		const FIntPoint& Header = NeighborHeaders[HeaderIndex];
		const int32 Start = Header.X;
		const int32 End = Start + Header.Y;

		for (int32 i = Start; i < End; ++i)
		{
			if (AllNeighbors.IsValidIndex(i) && AllNeighbors[i] == NeighborModuleIndex)
			{
				return true;
			}
		}
		return false;
	}
};

/**
 * Compiled bonding rules optimized for runtime solving.
 * This is a plain struct (not UObject) because it's generated at runtime
 * and Compile() may be called off the game thread where NewObject is unsafe.
 */
struct PCGEXELEMENTSVALENCY_API FPCGExValencyBondingRulesCompiled
{
	/** Total number of modules */
	int32 ModuleCount = 0;

	/** Module weights (parallel array) */
	TArray<float> ModuleWeights;

	/** Module orbital masks per layer (Index = ModuleIndex * LayerCount + LayerIndex) */
	TArray<int64> ModuleOrbitalMasks;

	/** Module boundary orbital masks per layer - orbitals that must have NO neighbor (Index = ModuleIndex * LayerCount + LayerIndex) */
	TArray<int64> ModuleBoundaryMasks;

	/** Module wildcard orbital masks per layer - orbitals that must have ANY neighbor (Index = ModuleIndex * LayerCount + LayerIndex) */
	TArray<int64> ModuleWildcardMasks;

	/** Module min spawn constraints */
	TArray<int32> ModuleMinSpawns;

	/** Module max spawn constraints (-1 = unlimited) */
	TArray<int32> ModuleMaxSpawns;

	/** Module asset references */
	TArray<TSoftObjectPtr<UObject>> ModuleAssets;

	/** Module asset types for routing to appropriate spawner */
	TArray<EPCGExValencyAssetType> ModuleAssetTypes;

	/** Module names for fixed pick lookup (parallel to ModuleWeights) */
	TArray<FName> ModuleNames;

	/**
	 * Per-module local transform headers.
	 * X = start index in AllLocalTransforms, Y = count of transforms.
	 * Allows multiple transform variants per module for random selection.
	 */
	TArray<FIntPoint> ModuleLocalTransformHeaders;

	/** Flattened array of all local transforms for all modules */
	TArray<FTransform> AllLocalTransforms;

	/** Whether each module has local transform offsets */
	TArray<bool> ModuleHasLocalTransform;

	/** Get a random local transform for a module based on seed */
	FTransform GetModuleLocalTransform(int32 ModuleIndex, int32 Seed) const
	{
		if (!ModuleLocalTransformHeaders.IsValidIndex(ModuleIndex))
		{
			return FTransform::Identity;
		}

		const FIntPoint& Header = ModuleLocalTransformHeaders[ModuleIndex];
		if (Header.Y == 0)
		{
			return FTransform::Identity;
		}

		// Use seed to select a transform variant
		const int32 VariantIndex = FMath::Abs(Seed) % Header.Y;
		const int32 TransformIndex = Header.X + VariantIndex;

		if (AllLocalTransforms.IsValidIndex(TransformIndex))
		{
			return AllLocalTransforms[TransformIndex];
		}

		return FTransform::Identity;
	}

	/** Get the number of transform variants for a module */
	int32 GetModuleTransformCount(int32 ModuleIndex) const
	{
		return ModuleLocalTransformHeaders.IsValidIndex(ModuleIndex) ? ModuleLocalTransformHeaders[ModuleIndex].Y : 0;
	}

	/** Compiled layer data */
	TArray<FPCGExValencyLayerCompiled> Layers;

	/** Compiled patterns for post-solve pattern matching */
	FPCGExValencyPatternSetCompiled CompiledPatterns;

	/**
	 * Fast lookup: OrbitalMask -> array of candidate module indices.
	 * Key is the combined mask from all layers (for single-layer, just the mask).
	 * This allows O(1) lookup of which modules could potentially fit a node.
	 */
	TMap<int64, TArray<int32>> MaskToCandidates;

	/** Get layer count */
	int32 GetLayerCount() const { return Layers.Num(); }

	/** Get a module's orbital mask for a specific layer */
	int64 GetModuleOrbitalMask(int32 ModuleIndex, int32 LayerIndex) const
	{
		const int32 Index = ModuleIndex * Layers.Num() + LayerIndex;
		return ModuleOrbitalMasks.IsValidIndex(Index) ? ModuleOrbitalMasks[Index] : 0;
	}

	/** Get a module's boundary orbital mask for a specific layer (orbitals that must have no neighbor) */
	int64 GetModuleBoundaryMask(int32 ModuleIndex, int32 LayerIndex) const
	{
		const int32 Index = ModuleIndex * Layers.Num() + LayerIndex;
		return ModuleBoundaryMasks.IsValidIndex(Index) ? ModuleBoundaryMasks[Index] : 0;
	}

	/** Get a module's wildcard orbital mask for a specific layer (orbitals that must have any neighbor) */
	int64 GetModuleWildcardMask(int32 ModuleIndex, int32 LayerIndex) const
	{
		const int32 Index = ModuleIndex * Layers.Num() + LayerIndex;
		return ModuleWildcardMasks.IsValidIndex(Index) ? ModuleWildcardMasks[Index] : 0;
	}

	/** Build the MaskToCandidates lookup table */
	void BuildCandidateLookup();
};

/**
 * Main bonding rules data asset - the user-facing configuration.
 * Contains orbital set references and module definitions.
 */
UCLASS(BlueprintType, DisplayName="[PCGEx] Valency Orbital Set")
class PCGEXELEMENTSVALENCY_API UPCGExValencyBondingRules : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Orbital sets defining the layers. Each set defines orbitals for one layer.
	 * Using TObjectPtr ensures all orbital sets are loaded when this asset is loaded,
	 * avoiding the need for LoadSynchronous during Compile().
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Valency|Layers")
	TArray<TObjectPtr<UPCGExValencyOrbitalSet>> OrbitalSets;

	/** Module definitions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Valency|Modules")
	TArray<FPCGExValencyModuleDefinition> Modules;

	/**
	 * Generated mesh collection containing all mesh-type modules.
	 * Auto-created during BuildFromCages, never user-edited.
	 * Used by downstream spawners via FPickPacker/FPickUnpacker.
	 */
	UPROPERTY(Instanced)
	TObjectPtr<UPCGExMeshCollection> GeneratedMeshCollection;

	/**
	 * Generated actor collection containing all actor-type modules.
	 * Auto-created during BuildFromCages, never user-edited.
	 * Used by downstream spawners via FPickPacker/FPickUnpacker.
	 */
	UPROPERTY(Instanced)
	TObjectPtr<UPCGExActorCollection> GeneratedActorCollection;

	/**
	 * Mapping from module index to mesh collection entry index.
	 * Only valid entries have their index stored; others are -1.
	 */
	UPROPERTY()
	TArray<int32> ModuleToMeshEntryIndex;

	/**
	 * Mapping from module index to actor collection entry index.
	 * Only valid entries have their index stored; others are -1.
	 */
	UPROPERTY()
	TArray<int32> ModuleToActorEntryIndex;

	/**
	 * Transient: Discovered material variants per mesh asset.
	 * Set by builder during cage scanning, consumed by RebuildGeneratedCollections.
	 * Key = mesh asset soft object path, Value = array of discovered variants.
	 * Not serialized - regenerated on each build.
	 */
	TMap<FSoftObjectPath, TArray<FPCGExValencyMaterialVariant>> DiscoveredMaterialVariants;

	// ========== Patterns ==========

	/**
	 * Compiled patterns for post-solve pattern matching.
	 * Built from pattern cages during BuildFromCages, serialized with the asset.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Valency|Patterns")
	FPCGExValencyPatternSetCompiled Patterns;

	// ========== Build Metadata ==========

	/**
	 * Path of the level where this ruleset was last built.
	 * Used to warn when rebuilding from a different level.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Build Info")
	FString LastBuildLevelPath;

	/**
	 * Name of the volume that triggered the last build.
	 * For informational purposes only.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Build Info")
	FString LastBuildVolumeName;

	/**
	 * Timestamp of the last build operation.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Build Info")
	FDateTime LastBuildTime;

	/** Compiled runtime data (generated, not serialized) */
	TSharedPtr<FPCGExValencyBondingRulesCompiled> CompiledData;

	/** Get layer count */
	int32 GetLayerCount() const { return OrbitalSets.Num(); }

	/** Get module count */
	int32 GetModuleCount() const { return Modules.Num(); }

	/** Get pattern count */
	int32 GetPatternCount() const { return Patterns.GetPatternCount(); }

	/** Check if there are any patterns */
	bool HasPatterns() const { return Patterns.HasPatterns(); }

	/** Find an orbital set by layer name */
	const UPCGExValencyOrbitalSet* FindOrbitalSet(const FName& LayerName) const
	{
		for (const TObjectPtr<UPCGExValencyOrbitalSet>& OrbitalSet : OrbitalSets)
		{
			if (OrbitalSet && OrbitalSet->LayerName == LayerName)
			{
				return OrbitalSet;
			}
		}
		return nullptr;
	}

	/** Find a module by asset */
	FPCGExValencyModuleDefinition* FindModuleByAsset(const TSoftObjectPtr<UObject>& Asset)
	{
		for (FPCGExValencyModuleDefinition& Module : Modules)
		{
			if (Module.Asset == Asset)
			{
				return &Module;
			}
		}
		return nullptr;
	}

	/** Get or create a module for an asset */
	FPCGExValencyModuleDefinition& GetOrCreateModule(const TSoftObjectPtr<UObject>& Asset)
	{
		if (FPCGExValencyModuleDefinition* Existing = FindModuleByAsset(Asset))
		{
			return *Existing;
		}

		FPCGExValencyModuleDefinition& NewModule = Modules.AddDefaulted_GetRef();
		NewModule.Asset = Asset;
		return NewModule;
	}

	/**
	 * Compile the bonding rules for runtime use.
	 * Validates orbital sets (already loaded via TObjectPtr), assigns module indices,
	 * and builds optimized lookup structures.
	 * Thread-safe - can be called from any thread.
	 * @return True if compilation succeeded
	 */
	bool Compile();

	/** Check if the bonding rules have valid compiled data */
	bool IsCompiled() const { return CompiledData.IsValid(); }

	/**
	 * Rebuild the generated collections from current modules.
	 * Creates/updates GeneratedMeshCollection and GeneratedActorCollection
	 * based on module asset types. Called automatically by builder.
	 */
	void RebuildGeneratedCollections();

	/** Get the generated mesh collection (may be nullptr if no mesh modules) */
	UPCGExMeshCollection* GetMeshCollection() const { return GeneratedMeshCollection; }

	/** Get the generated actor collection (may be nullptr if no actor modules) */
	UPCGExActorCollection* GetActorCollection() const { return GeneratedActorCollection; }

	/** Get the mesh collection entry index for a module (-1 if not a mesh module) */
	int32 GetMeshEntryIndex(int32 ModuleIndex) const
	{
		return ModuleToMeshEntryIndex.IsValidIndex(ModuleIndex) ? ModuleToMeshEntryIndex[ModuleIndex] : -1;
	}

	/** Get the actor collection entry index for a module (-1 if not an actor module) */
	int32 GetActorEntryIndex(int32 ModuleIndex) const
	{
		return ModuleToActorEntryIndex.IsValidIndex(ModuleIndex) ? ModuleToActorEntryIndex[ModuleIndex] : -1;
	}

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface

	virtual void EDITOR_RegisterTrackingKeys(FPCGExContext* Context) const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
