// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGData.h"

#include "PCGExValencyCommon.generated.h"

/**
 * Type of asset referenced by a Valency module.
 * Used to route to appropriate spawner (mesh vs actor vs data asset).
 */
UENUM(BlueprintType)
enum class EPCGExValencyAssetType : uint8
{
	/** Type not yet determined or unknown */
	Unknown = 0,
	/** Static mesh asset (UStaticMesh) */
	Mesh,
	/** Actor class or Blueprint (spawned as actor) */
	Actor,
	/** PCG Data Asset (UPCGDataAsset) */
	DataAsset
};

namespace PCGExValency
{
	/** Algorithm state constants */
	namespace SlotState
	{
		constexpr int32 UNSET = -1;        // Not yet resolved
		constexpr int32 NULL_SLOT = -2;    // Boundary / no neighbor exists
		constexpr int32 UNSOLVABLE = -3;   // Contradiction detected
		constexpr int32 PLACEHOLDER = -4;  // For ligature replacement
	}

	/** Pin labels */
	namespace Labels
	{
		const FName SourceBondingRulesLabel = FName("BondingRules");
		const FName SourceSolverLabel = FName("Solver");
		const FName SourceClustersLabel = FName("Clusters");
		const FName OutputStagedLabel = FName("Staged");
	}

	/**
	 * Minimal per-node state for valency processing.
	 * Contains node identity and solver output only.
	 * Orbital data is accessed via FOrbitalCache.
	 */
	struct PCGEXELEMENTSVALENCY_API FValencyState
	{
		/** Index in the cluster (node-space, not point-space) */
		int32 NodeIndex = -1;

		/** Output: resolved module index, or special SlotState value */
		int32 ResolvedModule = SlotState::UNSET;

		/** Check if this state has been resolved (success, boundary, or unsolvable) */
		FORCEINLINE bool IsResolved() const { return ResolvedModule >= 0 || ResolvedModule == SlotState::NULL_SLOT || ResolvedModule == SlotState::UNSOLVABLE; }

		/** Check if this is a boundary state (no orbitals, marked NULL) */
		FORCEINLINE bool IsBoundary() const { return ResolvedModule == SlotState::NULL_SLOT; }

		/** Check if this state failed to solve (contradiction) */
		FORCEINLINE bool IsUnsolvable() const { return ResolvedModule == SlotState::UNSOLVABLE; }
	};
}


/**
 * Shared module settings - used on cages and in module definitions.
 * Cages are the source of truth; BondingRules are compiled from cages.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCY_API FPCGExValencyModuleSettings
{
	GENERATED_BODY()

	/** Probability weight for selection (higher = more likely) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ClampMin = "0.001", PCGEX_ValencyRebuild))
	float Weight = 1.0f;

	/** Minimum number of times this module must be placed (0 = no minimum) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ClampMin = "0", PCGEX_ValencyRebuild))
	int32 MinSpawns = 0;

	/** Maximum number of times this module can be placed (-1 = unlimited) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ClampMin = "-1", PCGEX_ValencyRebuild))
	int32 MaxSpawns = -1;
};

/**
 * A single material override entry (slot index + material).
 * Used during material variant discovery.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCY_API FPCGExValencyMaterialOverride
{
	GENERATED_BODY()

	/** Material slot index */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	int32 SlotIndex = 0;

	/** The override material */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	TSoftObjectPtr<UMaterialInterface> Material;

	bool operator==(const FPCGExValencyMaterialOverride& Other) const
	{
		return SlotIndex == Other.SlotIndex && Material == Other.Material;
	}
};

/**
 * A discovered material variant configuration.
 * Represents a unique material configuration seen on a mesh during cage scanning.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCY_API FPCGExValencyMaterialVariant
{
	GENERATED_BODY()

	/** Material overrides for this variant (slot → material) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	TArray<FPCGExValencyMaterialOverride> Overrides;

	/** Discovery count - how many times this configuration was seen (becomes weight) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	int32 DiscoveryCount = 1;

	bool operator==(const FPCGExValencyMaterialVariant& Other) const
	{
		if (Overrides.Num() != Other.Overrides.Num()) { return false; }
		for (int32 i = 0; i < Overrides.Num(); ++i)
		{
			if (!(Overrides[i] == Other.Overrides[i])) { return false; }
		}
		return true;
	}
};

/**
 * An asset entry within a cage, with optional local transform.
 * When bPreserveLocalTransform is enabled on the cage, the LocalTransform
 * represents the asset's position relative to the cage center.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCY_API FPCGExValencyAssetEntry
{
	GENERATED_BODY()

	/** The asset (mesh, blueprint, actor class, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset")
	TSoftObjectPtr<UObject> Asset;

	/** Detected type of the asset (for routing to appropriate spawner) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Asset")
	EPCGExValencyAssetType AssetType = EPCGExValencyAssetType::Unknown;

	/** Transform relative to cage center (used when cage has bPreserveLocalTransforms enabled) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset")
	FTransform LocalTransform = FTransform::Identity;

	/** Source actor this entry was scanned from (transient, not saved) */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> SourceActor;

	/**
	 * Material variant for this specific entry instance.
	 * Stores the material overrides seen on this particular asset placement.
	 * Populated during cage scanning when materials differ from mesh defaults.
	 */
	UPROPERTY(Transient)
	FPCGExValencyMaterialVariant MaterialVariant;

	/** Whether this entry has a non-default material configuration */
	UPROPERTY(Transient)
	bool bHasMaterialVariant = false;

	/**
	 * Module settings (weight, spawn constraints) for this entry.
	 * Populated from the source cage/palette's ModuleSettings during collection.
	 * When mirroring, carries the SOURCE's settings (not the primary cage's).
	 */
	UPROPERTY(Transient)
	FPCGExValencyModuleSettings Settings;

	/** Whether this entry has custom settings (vs using defaults) */
	UPROPERTY(Transient)
	bool bHasSettings = false;

	bool IsValid() const { return !Asset.IsNull(); }
};

/**
 * Wrapper for array of module indices (needed for TMap UPROPERTY support).
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCY_API FPCGExValencyNeighborIndices
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neighbors")
	TArray<int32> Indices;

	void Add(int32 Index) { Indices.Add(Index); }
	void AddUnique(int32 Index) { Indices.AddUnique(Index); }
	int32 Num() const { return Indices.Num(); }
	bool Contains(int32 Index) const { return Indices.Contains(Index); }
};

/**
 * Per-layer orbital configuration for a module.
 * Stores which orbitals this module has and which neighbors are valid for each orbital.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCY_API FPCGExValencyModuleLayerConfig
{
	GENERATED_BODY()

	/** Bitmask indicating which orbitals this module has (bits set = orbital exists) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	int64 OrbitalMask = 0;

	/** Bitmask indicating which orbitals connect to boundaries/null cages (no neighbors expected) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	int64 BoundaryOrbitalMask = 0;

	/** Valid neighbor module indices per orbital. Key = OrbitalName, Value = array of valid module indices */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	TMap<FName, FPCGExValencyNeighborIndices> OrbitalNeighbors;

	/** Check if this module has a specific orbital */
	bool HasOrbital(int32 BitIndex) const
	{
		return (OrbitalMask & (1LL << BitIndex)) != 0;
	}

	/** Check if an orbital connects to a boundary (null cage) */
	bool IsBoundaryOrbital(int32 BitIndex) const
	{
		return (BoundaryOrbitalMask & (1LL << BitIndex)) != 0;
	}

	/** Set an orbital as present */
	void SetOrbital(int32 BitIndex)
	{
		OrbitalMask |= (1LL << BitIndex);
	}

	/** Mark an orbital as connecting to a boundary */
	void SetBoundaryOrbital(int32 BitIndex)
	{
		BoundaryOrbitalMask |= (1LL << BitIndex);
	}

	/** Add a valid neighbor for an orbital */
	void AddValidNeighbor(const FName& OrbitalName, int32 NeighborModuleIndex)
	{
		FPCGExValencyNeighborIndices& Neighbors = OrbitalNeighbors.FindOrAdd(OrbitalName);
		Neighbors.AddUnique(NeighborModuleIndex);
	}
};

/**
 * A module definition - represents one placeable asset with its orbital configuration.
 * Modules are uniquely identified by Asset + OrbitalMask + LocalTransform combination.
 * Same asset with different connectivity or placement = different modules.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSVALENCY_API FPCGExValencyModuleDefinition
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	/**
	 * Display name for this module variant (auto-generated).
	 * Helps identify modules during review. E.g., "Cube_NE_4conn"
	 */
	FString VariantName;
#endif

	/** The asset to spawn (mesh, actor, data asset, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module")
	TSoftObjectPtr<UObject> Asset;

	/** Type of asset (for routing to appropriate spawner) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Module")
	EPCGExValencyAssetType AssetType = EPCGExValencyAssetType::Unknown;

	/**
	 * Local transform relative to spawn point.
	 * Used when the source cage had bPreserveLocalTransforms enabled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module")
	FTransform LocalTransform = FTransform::Identity;

	/** Whether this module uses a local transform offset */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Module")
	bool bHasLocalTransform = false;

	/**
	 * Material variant for this specific module.
	 * Stored directly on the module so each module has its own unique material configuration.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module")
	FPCGExValencyMaterialVariant MaterialVariant;

	/** Whether this module has a material variant (non-default materials) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Module")
	bool bHasMaterialVariant = false;

	/** Module settings (weight, spawn constraints) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module")
	FPCGExValencyModuleSettings Settings;

	/** Per-layer orbital configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Module")
	TMap<FName, FPCGExValencyModuleLayerConfig> Layers;

	/** Check if this module can still be spawned given current spawn count */
	bool CanSpawn(int32 CurrentSpawnCount) const
	{
		return Settings.MaxSpawns < 0 || CurrentSpawnCount < Settings.MaxSpawns;
	}

	/** Check if this module needs more spawns to meet minimum */
	bool NeedsMoreSpawns(int32 CurrentSpawnCount) const
	{
		return CurrentSpawnCount < Settings.MinSpawns;
	}

	/** Get a unique key for this module (Asset path + primary orbital mask) */
	FString GetModuleKey(const FName& PrimaryLayerName) const
	{
		const FPCGExValencyModuleLayerConfig* LayerConfig = Layers.Find(PrimaryLayerName);
		const int64 Mask = LayerConfig ? LayerConfig->OrbitalMask : 0;
		return FString::Printf(TEXT("%s_%lld"), *Asset.ToSoftObjectPath().ToString(), Mask);
	}
};
