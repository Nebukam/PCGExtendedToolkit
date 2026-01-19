// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "EdMode.h"
#include "EditorMode/PCGExValencyAssetTracker.h"

class APCGExValencyCageBase;
class APCGExValencyCage;
class APCGExValencyAssetPalette;
class AValencyContextVolume;

/**
 * Editor mode for Valency Cage authoring.
 * Provides viewport visualization of orbital connections, cage states, and placement tools.
 *
 * This class orchestrates:
 * - Cache management for cages and volumes
 * - Visualization via FPCGExValencyDrawHelper
 * - Asset tracking via FPCGExValencyAssetTracker
 * - Input handling and actor lifecycle events
 *
 * Configuration is stored in UPCGExValencyEditorSettings (Project Settings > Plugins > PCGEx Valency Editor).
 */
class PCGEXELEMENTSVALENCYEDITOR_API FPCGExValencyCageEditorMode : public FEdMode
{
public:
	/** Mode identifier */
	static const FEditorModeID ModeID;

	FPCGExValencyCageEditorMode();
	virtual ~FPCGExValencyCageEditorMode() override;

	//~ Begin FEdMode Interface
	virtual void Enter() override;
	virtual void Exit() override;
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;
	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	virtual bool IsSelectionAllowed(AActor* InActor, bool bInSelection) const override;
	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
	//~ End FEdMode Interface

	/** Get the cached cages array */
	const TArray<TWeakObjectPtr<APCGExValencyCageBase>>& GetCachedCages() const { return CachedCages; }

	/** Get the cached volumes array */
	const TArray<TWeakObjectPtr<AValencyContextVolume>>& GetCachedVolumes() const { return CachedVolumes; }

	/** Get the cached palettes array */
	const TArray<TWeakObjectPtr<APCGExValencyAssetPalette>>& GetCachedPalettes() const { return CachedPalettes; }

protected:
	// ========== Cache Management ==========

	/** Collect all cages in the current level */
	void CollectCagesFromLevel();

	/** Collect all volumes in the current level */
	void CollectVolumesFromLevel();

	/** Collect all asset palettes in the current level */
	void CollectPalettesFromLevel();

	/** Master refresh - ensures all cages are properly initialized and connected */
	void RefreshAllCages();

	/** Initialize a single cage's orbitals and detect its connections */
	void InitializeCage(APCGExValencyCageBase* Cage);

	// ========== Actor Lifecycle ==========

	/** Callback when an actor is added to the level */
	void OnLevelActorAdded(AActor* Actor);

	/** Callback when an actor is deleted from the level */
	void OnLevelActorDeleted(AActor* Actor);

	/** Callback when selection changes */
	void OnSelectionChanged();

	// ========== Utilities ==========

	/** Set visibility of all cage debug components */
	void SetAllCageDebugComponentsVisible(bool bVisible);

	/** Cleanup stale manual connections from all cages */
	int32 CleanupAllManualConnections();

	/** Redraw all viewports and invalidate viewport clients */
	void RedrawViewports();

private:
	// ========== Cache State ==========

	/** Cached cages in level */
	TArray<TWeakObjectPtr<APCGExValencyCageBase>> CachedCages;

	/** Cached volumes in level */
	TArray<TWeakObjectPtr<AValencyContextVolume>> CachedVolumes;

	/** Cached asset palettes in level */
	TArray<TWeakObjectPtr<APCGExValencyAssetPalette>> CachedPalettes;

	/** Whether cache needs refresh */
	bool bCacheDirty = true;

	// ========== Delegate Handles ==========

	FDelegateHandle OnActorAddedHandle;
	FDelegateHandle OnActorDeletedHandle;
	FDelegateHandle OnSelectionChangedHandle;

	// ========== Asset Tracking ==========

	/** Asset tracker for monitoring selected actors */
	FPCGExValencyAssetTracker AssetTracker;
};
