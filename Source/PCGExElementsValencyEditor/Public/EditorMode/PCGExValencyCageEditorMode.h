// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Tools/UEdMode.h"
#include "Tools/LegacyEdModeInterfaces.h"
#include "EditorMode/PCGExValencyAssetTracker.h"
#include "EditorMode/PCGExValencyDirtyState.h"
#include "EditorMode/PCGExValencyReferenceTracker.h"

#include "PCGExValencyCageEditorMode.generated.h"

class APCGExValencyCageBase;
class APCGExValencyCage;
class APCGExValencyAssetPalette;
class AValencyContextVolume;
class FPCGExValencyEditorModeToolkit;

/** Delegate fired when the scene cache (cages/volumes/palettes) changes */
DECLARE_MULTICAST_DELEGATE(FOnValencySceneChanged);

/**
 * Visibility flags for controlling which visualization layers are rendered.
 * Persists for the duration of the editor mode session.
 */
struct FValencyVisibilityFlags
{
	bool bShowConnections = true;
	bool bShowLabels = true;
	bool bShowSockets = true;
	bool bShowVolumes = true;
	bool bShowGhostMeshes = true;
	bool bShowPatterns = true;
};

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
UCLASS()
class PCGEXELEMENTSVALENCYEDITOR_API UPCGExValencyCageEditorMode
	: public UEdMode
	, public ILegacyEdModeWidgetInterface
	, public ILegacyEdModeViewportInterface
{
	GENERATED_BODY()

public:
	/** Mode identifier */
	static const FEditorModeID ModeID;

	UPCGExValencyCageEditorMode();

	//~ Begin UEdMode Interface
	virtual void Enter() override;
	virtual void Exit() override;
	virtual void ModeTick(float DeltaTime) override;
	virtual bool IsSelectionAllowed(AActor* InActor, bool bInSelection) const override;
	//~ End UEdMode Interface

protected:
	virtual void CreateToolkit() override;

public:
	//~ Begin ILegacyEdModeWidgetInterface
	virtual bool AllowWidgetMove() override { return true; }
	virtual bool CanCycleWidgetMode() const override { return false; }
	virtual bool ShowModeWidgets() const override { return true; }
	virtual EAxisList::Type GetWidgetAxisToDraw(UE::Widget::EWidgetMode InWidgetMode) const override { return EAxisList::All; }
	virtual FVector GetWidgetLocation() const override { return FVector::ZeroVector; }
	virtual bool ShouldDrawWidget() const override { return false; }
	virtual bool UsesTransformWidget() const override { return true; }
	virtual bool UsesTransformWidget(UE::Widget::EWidgetMode CheckMode) const override { return true; }
	virtual FVector GetWidgetNormalFromCurrentAxis(void* InData) override { return FVector(0, 0, 1); }
	virtual void SetCurrentWidgetAxis(EAxisList::Type InAxis) override { WidgetAxis = InAxis; }
	virtual EAxisList::Type GetCurrentWidgetAxis() const override { return WidgetAxis; }
	virtual bool UsesPropertyWidgets() const override { return false; }
	virtual bool GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData) override { return false; }
	virtual bool GetCustomInputCoordinateSystem(FMatrix& InMatrix, void* InData) override { return false; }
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;
	//~ End ILegacyEdModeWidgetInterface

	//~ Begin ILegacyEdModeViewportInterface
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;
	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	//~ End ILegacyEdModeViewportInterface

	/** Get the cached cages array */
	const TArray<TWeakObjectPtr<APCGExValencyCageBase>>& GetCachedCages() const { return CachedCages; }

	/** Get the cached volumes array */
	const TArray<TWeakObjectPtr<AValencyContextVolume>>& GetCachedVolumes() const { return CachedVolumes; }

	/** Get the cached palettes array */
	const TArray<TWeakObjectPtr<APCGExValencyAssetPalette>>& GetCachedPalettes() const { return CachedPalettes; }

	/** Get the dirty state manager for marking actors dirty */
	FValencyDirtyStateManager& GetDirtyStateManager() { return DirtyStateManager; }

	/** Get the reference tracker for change propagation */
	FValencyReferenceTracker& GetReferenceTracker() { return ReferenceTracker; }

	/** Get the visualization visibility flags */
	const FValencyVisibilityFlags& GetVisibilityFlags() const { return VisibilityFlags; }

	/** Get mutable visibility flags (for toggle widgets) */
	FValencyVisibilityFlags& GetMutableVisibilityFlags() { return VisibilityFlags; }

	/** Delegate fired when the scene cache changes (cages/volumes/palettes added/removed) */
	FOnValencySceneChanged OnSceneChanged;

	/**
	 * Get the reference tracker from the active Valency editor mode.
	 * @return Pointer to tracker if mode is active, nullptr otherwise
	 */
	static FValencyReferenceTracker* GetActiveReferenceTracker();

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

	/** Callback after Undo/Redo operation completes */
	void OnPostUndoRedo();

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

	// ========== Visualization ==========

	/** Visibility toggle state for visualization layers */
	FValencyVisibilityFlags VisibilityFlags;

	/** Widget axis for ILegacyEdModeWidgetInterface */
	EAxisList::Type WidgetAxis = EAxisList::None;

	// ========== Delegate Handles ==========

	FDelegateHandle OnActorAddedHandle;
	FDelegateHandle OnActorDeletedHandle;
	FDelegateHandle OnSelectionChangedHandle;
	FDelegateHandle OnPostUndoRedoHandle;

	// ========== Asset Tracking ==========

	/** Asset tracker for monitoring selected actors */
	FPCGExValencyAssetTracker AssetTracker;

	// ========== Dirty State Management ==========

	/** Central dirty state manager for coalesced rebuilds */
	FValencyDirtyStateManager DirtyStateManager;

	/** Skip dirty processing for one frame after mode entry (allows system to stabilize) */
	bool bSkipNextDirtyProcess = false;

	// ========== Reference Tracking ==========

	/** Central reference tracker for change propagation */
	FValencyReferenceTracker ReferenceTracker;
};
