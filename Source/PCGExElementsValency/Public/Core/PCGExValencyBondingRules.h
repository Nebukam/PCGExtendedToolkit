// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PCGExValencyCommon.h"
#include "PCGExValencyOrbitalSet.h"

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

	/** Module min spawn constraints */
	TArray<int32> ModuleMinSpawns;

	/** Module max spawn constraints (-1 = unlimited) */
	TArray<int32> ModuleMaxSpawns;

	/** Module asset references */
	TArray<TSoftObjectPtr<UObject>> ModuleAssets;

	/** Module asset types for routing to appropriate spawner */
	TArray<EPCGExValencyAssetType> ModuleAssetTypes;

	/** Module local transforms (relative to spawn point) */
	TArray<FTransform> ModuleLocalTransforms;

	/** Whether each module has a local transform offset */
	TArray<bool> ModuleHasLocalTransform;

	/** Compiled layer data */
	TArray<FPCGExValencyLayerCompiled> Layers;

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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Valency|Generated", Instanced)
	TObjectPtr<UPCGExMeshCollection> GeneratedMeshCollection;

	/**
	 * Generated actor collection containing all actor-type modules.
	 * Auto-created during BuildFromCages, never user-edited.
	 * Used by downstream spawners via FPickPacker/FPickUnpacker.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Valency|Generated", Instanced)
	TObjectPtr<UPCGExActorCollection> GeneratedActorCollection;

	/**
	 * Mapping from module index to mesh collection entry index.
	 * Only valid entries have their index stored; others are -1.
	 */
	TArray<int32> ModuleToMeshEntryIndex;

	/**
	 * Mapping from module index to actor collection entry index.
	 * Only valid entries have their index stored; others are -1.
	 */
	TArray<int32> ModuleToActorEntryIndex;

	/** Compiled runtime data (generated, not serialized) */
	TSharedPtr<FPCGExValencyBondingRulesCompiled> CompiledData;

	/** Get layer count */
	int32 GetLayerCount() const { return OrbitalSets.Num(); }

	/** Get module count */
	int32 GetModuleCount() const { return Modules.Num(); }

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

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
