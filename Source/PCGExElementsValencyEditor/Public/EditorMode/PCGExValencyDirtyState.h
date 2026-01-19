// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

class APCGExValencyCageBase;
class APCGExValencyCage;
class APCGExValencyAssetPalette;
class AValencyContextVolume;

/**
 * Flags indicating what aspects of a Valency actor are dirty.
 * Granular tracking allows targeted rebuilds.
 */
UENUM(Meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EValencyDirtyFlags : uint8
{
	None = 0,

	/** Asset entries changed (added, removed, or modified) */
	Assets = 1 << 0,

	/** Material variants changed */
	Materials = 1 << 1,

	/** Orbital connections changed */
	Orbitals = 1 << 2,

	/** Volume membership changed (cage moved in/out of volume) */
	VolumeMembership = 1 << 3,

	/** Module settings changed (weight, min/max spawns) */
	ModuleSettings = 1 << 4,

	/** Mirror sources changed */
	MirrorSources = 1 << 5,

	/** Transform changed (for local transform preservation) */
	Transform = 1 << 6,

	/** Structure changed (requires full rebuild) */
	Structure = Assets | Orbitals | VolumeMembership | MirrorSources,

	/** All flags */
	All = 0xFF
};
ENUM_CLASS_FLAGS(EValencyDirtyFlags);

/**
 * Represents a dirty actor with its associated flags.
 */
struct FValencyDirtyEntry
{
	TWeakObjectPtr<AActor> Actor;
	EValencyDirtyFlags Flags = EValencyDirtyFlags::None;

	FValencyDirtyEntry() = default;
	FValencyDirtyEntry(AActor* InActor, EValencyDirtyFlags InFlags)
		: Actor(InActor), Flags(InFlags)
	{
	}

	bool IsValid() const { return Actor.IsValid(); }
};

/**
 * Central dirty state manager for the Valency editor system.
 *
 * Design principles:
 * 1. Mark dirty immediately when changes occur (cheap)
 * 2. Process dirty state once per frame (coalesced)
 * 3. Cascade dirtiness through relationships (mirrors, volumes)
 * 4. Track granular dirty flags for targeted rebuilds
 *
 * Usage:
 *   - Call MarkDirty() whenever a cage/palette/volume changes
 *   - Call ProcessDirty() once per frame in EditorMode::Tick
 *   - IsDirty() for querying current state
 */
class PCGEXELEMENTSVALENCYEDITOR_API FValencyDirtyStateManager
{
public:
	FValencyDirtyStateManager() = default;
	~FValencyDirtyStateManager() = default;

	// Non-copyable
	FValencyDirtyStateManager(const FValencyDirtyStateManager&) = delete;
	FValencyDirtyStateManager& operator=(const FValencyDirtyStateManager&) = delete;

	/**
	 * Initialize the manager with references to cached actors.
	 * Must be called before using other methods.
	 */
	void Initialize(
		const TArray<TWeakObjectPtr<APCGExValencyCageBase>>& InCachedCages,
		const TArray<TWeakObjectPtr<AValencyContextVolume>>& InCachedVolumes,
		const TArray<TWeakObjectPtr<APCGExValencyAssetPalette>>& InCachedPalettes);

	/** Clear all dirty state */
	void Reset();

	//~ Dirty Marking API

	/** Mark a cage as dirty with specific flags */
	void MarkCageDirty(APCGExValencyCageBase* Cage, EValencyDirtyFlags Flags);

	/** Mark a palette as dirty with specific flags */
	void MarkPaletteDirty(APCGExValencyAssetPalette* Palette, EValencyDirtyFlags Flags);

	/** Mark a volume as dirty (needs rebuild) */
	void MarkVolumeDirty(AValencyContextVolume* Volume, EValencyDirtyFlags Flags);

	/** Mark all actors in a volume as dirty */
	void MarkVolumeContentsDirty(AValencyContextVolume* Volume, EValencyDirtyFlags Flags);

	//~ Dirty Query API

	/** Check if any dirty state is pending */
	bool HasDirtyState() const { return DirtyCages.Num() > 0 || DirtyPalettes.Num() > 0 || DirtyVolumes.Num() > 0; }

	/** Check if a specific cage is dirty */
	bool IsCageDirty(const APCGExValencyCageBase* Cage) const;

	/** Check if a specific palette is dirty */
	bool IsPaletteDirty(const APCGExValencyAssetPalette* Palette) const;

	/** Check if a specific volume is dirty */
	bool IsVolumeDirty(const AValencyContextVolume* Volume) const;

	/** Get dirty flags for a cage */
	EValencyDirtyFlags GetCageDirtyFlags(const APCGExValencyCageBase* Cage) const;

	//~ Processing API

	/**
	 * Process all pending dirty state.
	 * Called once per frame in EditorMode::Tick.
	 *
	 * @param bRebuildEnabled Whether to trigger actual rebuilds (respects bAutoRebuildOnChange)
	 * @return Number of volumes that were rebuilt
	 */
	int32 ProcessDirty(bool bRebuildEnabled = true);

	/**
	 * Expand dirty set to include mirror relationships.
	 * Cages that mirror dirty cages/palettes become dirty too.
	 */
	void ExpandDirtyThroughMirrors();

	/**
	 * Collect volumes that contain any dirty cages.
	 */
	void CollectAffectedVolumes(TSet<AValencyContextVolume*>& OutVolumes) const;

	//~ Debug API

	/** Get count of dirty cages */
	int32 GetDirtyCageCount() const { return DirtyCages.Num(); }

	/** Get count of dirty palettes */
	int32 GetDirtyPaletteCount() const { return DirtyPalettes.Num(); }

	/** Get count of dirty volumes */
	int32 GetDirtyVolumeCount() const { return DirtyVolumes.Num(); }

private:
	/** Find all cages that mirror the given actor (cage or palette) */
	void FindMirroringCages(AActor* SourceActor, TArray<APCGExValencyCage*>& OutMirroringCages) const;

	/** Refresh a dirty cage's scanned assets if needed */
	void RefreshCageIfNeeded(APCGExValencyCage* Cage, EValencyDirtyFlags Flags);

	/** Refresh a dirty palette's scanned assets if needed */
	void RefreshPaletteIfNeeded(APCGExValencyAssetPalette* Palette, EValencyDirtyFlags Flags);

private:
	/** Reference to cached cages (owned by editor mode) */
	const TArray<TWeakObjectPtr<APCGExValencyCageBase>>* CachedCages = nullptr;

	/** Reference to cached volumes (owned by editor mode) */
	const TArray<TWeakObjectPtr<AValencyContextVolume>>* CachedVolumes = nullptr;

	/** Reference to cached palettes (owned by editor mode) */
	const TArray<TWeakObjectPtr<APCGExValencyAssetPalette>>* CachedPalettes = nullptr;

	/** Dirty cages with their flags */
	TMap<TWeakObjectPtr<APCGExValencyCageBase>, EValencyDirtyFlags> DirtyCages;

	/** Dirty palettes with their flags */
	TMap<TWeakObjectPtr<APCGExValencyAssetPalette>, EValencyDirtyFlags> DirtyPalettes;

	/** Dirty volumes with their flags */
	TMap<TWeakObjectPtr<AValencyContextVolume>, EValencyDirtyFlags> DirtyVolumes;

	/** Flag to prevent recursive processing */
	bool bIsProcessing = false;
};
