// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "EdMode.h"
#include "Cages/PCGExValencyCageOrbital.h"

class APCGExValencyCageBase;
class AValencyContextVolume;
class UPCGExValencyOrbitalSet;

/**
 * Editor mode for Valency Cage authoring.
 * Provides viewport visualization of orbital connections, cage states, and placement tools.
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

	/** Main 3D rendering callback - draws orbital connections, cage visuals */
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;

	/** HUD rendering callback - draws labels, status text */
	virtual void DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;

	/** Handle viewport clicks for cage placement */
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;

	/** Process keyboard input */
	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;

	/** Allow actor selection */
	virtual bool IsSelectionAllowed(AActor* InActor, bool bInSelection) const override;

	/** Tick for any continuous updates */
	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
	//~ End FEdMode Interface

protected:
	/** Collect all cages in the current level */
	void CollectCagesFromLevel();

	/** Collect all volumes in the current level */
	void CollectVolumesFromLevel();

	/** Draw a single cage's visualization */
	void DrawCage(FPrimitiveDrawInterface* PDI, const APCGExValencyCageBase* Cage);

	/** Draw a volume's boundaries */
	void DrawVolume(FPrimitiveDrawInterface* PDI, const AValencyContextVolume* Volume);

	/** Draw connection line along orbital direction */
	void DrawConnection(FPrimitiveDrawInterface* PDI, const FVector& From, const FVector& Along, float Distance, const FLinearColor& Color, bool bDrawArrowhead = false, bool bDashed = false);

	/** Draw orbital direction arrow - from Start to End with optional arrowhead */
	void DrawOrbitalArrow(FPrimitiveDrawInterface* PDI, const FVector& Start, const FVector& End, const FLinearColor& Color, bool bDashed = false, bool bDrawArrowhead = true, float Thickness = 1.5f, float ThicknessArrow = 2.0f);

	/** Draw text label in viewport */
	void DrawLabel(FCanvas* Canvas, const FSceneView* View, const FVector& WorldLocation, const FString& Text, const FLinearColor& Color);

	/** Set visibility of all cage debug components */
	void SetAllCageDebugComponentsVisible(bool bVisible);

	/** Callback when an actor is added to the level */
	void OnLevelActorAdded(AActor* Actor);

	/** Callback when an actor is deleted from the level */
	void OnLevelActorDeleted(AActor* Actor);

	/**
	 * Master refresh function - ensures all cages are properly initialized and connected.
	 * Call this whenever state might be stale (mode enter, actor changes, etc.)
	 */
	void RefreshAllCages();

	/** Initialize a single cage's orbitals and detect its connections */
	void InitializeCage(APCGExValencyCageBase* Cage);

	/**
	 * Cleanup stale manual connections from all cages.
	 * Removes null/invalid entries from manual connection lists.
	 * @return Total number of stale entries removed across all cages
	 */
	int32 CleanupAllManualConnections();

private:
	/** Cached cages in level */
	TArray<TWeakObjectPtr<APCGExValencyCageBase>> CachedCages;

	/** Cached volumes in level */
	TArray<TWeakObjectPtr<AValencyContextVolume>> CachedVolumes;

	/** Whether cache needs refresh */
	bool bCacheDirty = true;

	/** Delegate handles for actor add/delete events */
	FDelegateHandle OnActorAddedHandle;
	FDelegateHandle OnActorDeletedHandle;

	/** Visualization settings */
	float OrbitalArrowLength = 100.0f;
	float ConnectionLineThickness = 0.5f;          // Thin center-to-center lines (many of them)
	float ArrowStartOffsetPct = 0.25f;             // Arrows start at 25% of probe radius from center
	float DashLength = 8.0f;                      // Length of each dash segment
	float DashGap = 12.0f;                          // Gap between dashes

	/** Colors */
	FLinearColor BidirectionalColor = FLinearColor(0.2f, 0.8f, 0.2f);      // Green - mutual connection
	FLinearColor UnilateralColor = FLinearColor(0.0f, 0.6f, 0.6f);         // Teal - one-way connection
	FLinearColor NullConnectionColor = FLinearColor(0.5f, 0.15f, 0.15f);   // Darkish red - connection to null
	FLinearColor NoConnectionColor = FLinearColor(0.6f, 0.6f, 0.6f);       // Light gray - no connection
	FLinearColor VolumeColor = FLinearColor(0.3f, 0.3f, 0.8f, 0.3f);
	FLinearColor WarningColor = FLinearColor(1.0f, 0.5f, 0.0f);
};
