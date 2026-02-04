// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

class APCGExValencyCageBase;
class APCGExValencyCage;
class APCGExValencyAssetPalette;
class AValencyContextVolume;

/**
 * Tracks selected actors and their containment in Valency cages.
 * Detects when actors move into/out of cages and triggers cage refreshes.
 *
 * This class owns all asset tracking state and is used by the editor mode.
 */
class PCGEXELEMENTSVALENCYEDITOR_API FPCGExValencyAssetTracker
{
public:
	FPCGExValencyAssetTracker() = default;
	~FPCGExValencyAssetTracker() = default;

	// Non-copyable
	FPCGExValencyAssetTracker(const FPCGExValencyAssetTracker&) = delete;
	FPCGExValencyAssetTracker& operator=(const FPCGExValencyAssetTracker&) = delete;

	/**
	 * Initialize the tracker with references to cached data.
	 * Must be called before using other methods.
	 * @param InCachedCages Reference to the cached cages array
	 * @param InCachedVolumes Reference to the cached volumes array
	 * @param InCachedPalettes Reference to the cached palettes array
	 */
	void Initialize(
		const TArray<TWeakObjectPtr<APCGExValencyCageBase>>& InCachedCages,
		const TArray<TWeakObjectPtr<AValencyContextVolume>>& InCachedVolumes,
		const TArray<TWeakObjectPtr<APCGExValencyAssetPalette>>& InCachedPalettes);

	/** Clear all tracking state */
	void Reset();

	/**
	 * Check if asset tracking is enabled on any volume.
	 * @return True if at least one volume has bAutoTrackAssetPlacement enabled
	 */
	bool IsEnabled() const;

	/**
	 * Called when the editor selection changes.
	 * Rebuilds the tracked actors list from the current selection.
	 */
	void OnSelectionChanged();

	/**
	 * Called when an actor is deleted from the level.
	 * Handles cleanup if the actor was being tracked.
	 * @param DeletedActor The actor being deleted
	 * @param OutAffectedCage Will be set to the cage that contained the deleted actor (if any)
	 * @return True if the deleted actor was tracked and a cage was affected
	 */
	bool OnActorDeleted(AActor* DeletedActor, APCGExValencyCage*& OutAffectedCage);

	/**
	 * Update tracking state - call every tick when enabled.
	 * Checks for position changes and containment changes.
	 * @param OutAffectedCages Set that will be populated with cages that need refresh
	 * @param OutAffectedPalettes Set that will be populated with palettes that need refresh
	 * @return True if any cages or palettes were affected
	 */
	bool Update(TSet<APCGExValencyCage*>& OutAffectedCages, TSet<APCGExValencyAssetPalette*>& OutAffectedPalettes);

	/** Get the number of currently tracked actors */
	int32 GetTrackedActorCount() const { return TrackedActors.Num(); }

private:
	/** Check if an actor should be ignored based on volume ignore rules */
	bool ShouldIgnoreActor(AActor* Actor) const;

	/** Find which cage contains an actor (or nullptr if none) */
	APCGExValencyCage* FindContainingCage(AActor* Actor) const;

	/** Find which palette contains an actor (or nullptr if none) */
	APCGExValencyAssetPalette* FindContainingPalette(AActor* Actor) const;

	/** Collect non-null cages that can receive assets */
	void CollectTrackingCages(TArray<APCGExValencyCage*>& OutCages) const;

	/** Collect palettes that can receive assets */
	void CollectTrackingPalettes(TArray<APCGExValencyAssetPalette*>& OutPalettes) const;

	/** Find all cages that mirror the given cage (have it in their MirrorSources) */
	void FindCagesThatMirror(APCGExValencyCage* SourceCage, TArray<APCGExValencyCage*>& OutMirroringCages) const;

private:
	/** Reference to cached cages (owned by editor mode) */
	const TArray<TWeakObjectPtr<APCGExValencyCageBase>>* CachedCages = nullptr;

	/** Reference to cached volumes (owned by editor mode) */
	const TArray<TWeakObjectPtr<AValencyContextVolume>>* CachedVolumes = nullptr;

	/** Reference to cached palettes (owned by editor mode) */
	const TArray<TWeakObjectPtr<APCGExValencyAssetPalette>>* CachedPalettes = nullptr;

	/** Actors currently being tracked (selected non-cage actors) */
	TArray<TWeakObjectPtr<AActor>> TrackedActors;

	/** Map from tracked actor to its last known containing cage */
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<APCGExValencyCage>> TrackedActorCageMap;

	/** Map from tracked actor to its last known containing palette */
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<APCGExValencyAssetPalette>> TrackedActorPaletteMap;

	/** Last known transforms of tracked actors (for detecting rotation/scale changes) */
	TMap<TWeakObjectPtr<AActor>, FTransform> TrackedActorTransforms;
};
