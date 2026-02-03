// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class APCGExValencyCageBase;
class UWorld;

/**
 * Spatial registry for efficient cage neighbor queries.
 * Uses a grid-based spatial hash for O(1) cell lookups.
 *
 * This is a lightweight editor-only helper, not a UObject.
 */
class PCGEXELEMENTSVALENCYEDITOR_API FPCGExValencyCageSpatialRegistry
{
public:
	/** Get the registry for a world (creates if needed) */
	static FPCGExValencyCageSpatialRegistry& Get(UWorld* World);

	/** Clear registry for a world */
	static void Clear(UWorld* World);

	/** Register a cage in the spatial hash */
	void RegisterCage(APCGExValencyCageBase* Cage);

	/** Unregister a cage from the spatial hash */
	void UnregisterCage(APCGExValencyCageBase* Cage);

	/** Update a cage's position in the spatial hash */
	void UpdateCagePosition(APCGExValencyCageBase* Cage, const FVector& OldPosition, const FVector& NewPosition);

	/**
	 * Find all cages that could potentially interact with a position.
	 * Returns cages within MaxQueryRadius of the position, OR cages whose probe radius reaches the position.
	 * @param Position World position to query
	 * @param MaxQueryRadius Maximum distance to search (should be >= largest probe radius in scene)
	 * @param OutCages Array to fill with found cages
	 * @param ExcludeCage Optional cage to exclude from results
	 */
	void FindCagesNearPosition(const FVector& Position, float MaxQueryRadius, TArray<APCGExValencyCageBase*>& OutCages, APCGExValencyCageBase* ExcludeCage = nullptr) const;

	/**
	 * Find cages affected by a cage moving from OldPos to NewPos.
	 * Includes:
	 * - Cages the moving cage can now reach
	 * - Cages the moving cage could previously reach but can't anymore
	 * - Cages that can reach the moving cage's new position
	 * - Cages that could reach the old position but can't reach the new one
	 */
	void FindAffectedCages(APCGExValencyCageBase* MovingCage, const FVector& OldPosition, const FVector& NewPosition, TSet<APCGExValencyCageBase*>& OutAffectedCages) const;

	/** Get the maximum probe radius across all registered cages */
	float GetMaxProbeRadius() const { return MaxProbeRadius; }

	/** Rebuild the entire registry from scratch */
	void RebuildFromWorld(UWorld* World);

	/** Set the grid cell size (affects performance vs accuracy tradeoff) */
	void SetCellSize(float NewCellSize);

	/** Constructor - use Get() to obtain instances */
	FPCGExValencyCageSpatialRegistry();

private:

	/** Convert world position to cell coordinates */
	FIntVector PositionToCell(const FVector& Position) const;

	/** Get cell key for hash map */
	uint64 GetCellKey(const FIntVector& Cell) const;

	/** Get all cells that overlap with a sphere */
	void GetOverlappingCells(const FVector& Center, float Radius, TArray<FIntVector>& OutCells) const;

	/** Recalculate max probe radius */
	void RecalculateMaxProbeRadius();

private:
	/** Grid cell size in world units */
	float CellSize = 200.0f;

	/** Spatial hash: cell key -> cages in that cell */
	TMap<uint64, TArray<TWeakObjectPtr<APCGExValencyCageBase>>> SpatialHash;

	/** All registered cages for iteration */
	TSet<TWeakObjectPtr<APCGExValencyCageBase>> AllCages;

	/** Cached maximum probe radius for query optimization */
	float MaxProbeRadius = 0.0f;

	/** Per-world registries */
	static TMap<TWeakObjectPtr<UWorld>, TSharedPtr<FPCGExValencyCageSpatialRegistry>> WorldRegistries;
};
