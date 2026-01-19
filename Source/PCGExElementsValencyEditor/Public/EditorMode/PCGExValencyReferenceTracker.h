// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

class APCGExValencyCageBase;
class APCGExValencyCage;
class APCGExValencyAssetPalette;
class AValencyContextVolume;

/**
 * Centralized reference tracking and change propagation for Valency actors.
 *
 * Maintains a pre-built dependency graph for O(1) dependent lookups.
 * When an actor's content changes, propagates updates to all dependents recursively.
 *
 * Extensible: Add new reference types by updating RebuildDependencyGraph().
 */
class PCGEXELEMENTSVALENCYEDITOR_API FValencyReferenceTracker
{
public:
	/**
	 * Initialize with cached actor lists from editor mode.
	 * Builds the dependency graph for fast lookups.
	 * Must be called during editor mode Enter().
	 */
	void Initialize(
		const TArray<TWeakObjectPtr<APCGExValencyCageBase>>* InCachedCages,
		const TArray<TWeakObjectPtr<AValencyContextVolume>>* InCachedVolumes,
		const TArray<TWeakObjectPtr<APCGExValencyAssetPalette>>* InCachedPalettes);

	/** Reset state. Called during editor mode Exit(). */
	void Reset();

	/**
	 * Rebuild the dependency graph.
	 * Call when actors are added/removed or when MirrorSources change.
	 */
	void RebuildDependencyGraph();

	/**
	 * Notify that an actor's references changed (e.g., MirrorSources modified).
	 * Rebuilds the dependency graph and propagates changes.
	 */
	void OnActorReferencesChanged(AActor* Actor);

	/**
	 * Notify that an actor's content has changed and propagate to all dependents.
	 * This is the main entry point for change propagation.
	 *
	 * @param ChangedActor The actor whose content changed (cage or palette)
	 * @param bRefreshGhosts Whether to refresh ghost meshes on dependent cages
	 * @param bTriggerRebuild Whether to trigger auto-rebuild for dependent cages
	 * @return True if any dependent was updated
	 */
	bool PropagateContentChange(AActor* ChangedActor, bool bRefreshGhosts = true, bool bTriggerRebuild = true);

	/**
	 * Get all actors that depend on the given actor (O(1) lookup).
	 * Uses pre-built dependency graph.
	 */
	const TArray<TWeakObjectPtr<AActor>>* GetDependents(AActor* Actor) const;

	/**
	 * Check if ActorA depends on ActorB (directly or indirectly).
	 * Useful for cycle detection and dependency analysis.
	 */
	bool DependsOn(AActor* ActorA, AActor* ActorB) const;

private:
	/**
	 * Collect all affected actors recursively (non-recursive iterative version).
	 * More efficient than recursive calls for deep chains.
	 */
	void CollectAffectedActors(AActor* StartActor, TArray<AActor*>& OutAffected);

	/**
	 * Refresh visual state for a dependent actor (ghost meshes, etc.).
	 */
	void RefreshDependentVisuals(AActor* Dependent);

	/**
	 * Trigger rebuild for a dependent actor through its volumes.
	 */
	bool TriggerDependentRebuild(AActor* Dependent);

	// Cached references to editor mode's actor lists
	const TArray<TWeakObjectPtr<APCGExValencyCageBase>>* CachedCages = nullptr;
	const TArray<TWeakObjectPtr<AValencyContextVolume>>* CachedVolumes = nullptr;
	const TArray<TWeakObjectPtr<APCGExValencyAssetPalette>>* CachedPalettes = nullptr;

	// Pre-built dependency graph: Actor → Actors that depend on it
	// Built once in Initialize(), updated via RebuildDependencyGraph()
	TMap<TWeakObjectPtr<AActor>, TArray<TWeakObjectPtr<AActor>>> DependentsMap;
};
