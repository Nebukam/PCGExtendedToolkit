// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "PCGExValencyEditorActorBase.generated.h"

class FValencyDirtyStateManager;

/**
 * Thin base class for all Valency editor actors (cages, palettes, etc.).
 * Provides centralized meta tag handling, dirty state management access,
 * and debug/rebuild hooks that subclasses override.
 *
 * Meta tag system:
 * - PCGEX_ValencyGhostRefresh: Calls OnGhostRefreshRequested() when a property with this meta changes
 * - PCGEX_ValencyRebuild: Calls OnRebuildMetaTagTriggered() (with debouncing) when a property with this meta changes
 * - OnPostEditChangeProperty: Called after meta tag handling for subclass-specific property logic
 */
UCLASS(Abstract, HideCategories = (Rendering, Replication, Collision, HLOD, Physics, Networking, Input, LOD, Cooking))
class PCGEXELEMENTSVALENCYEDITOR_API APCGExValencyEditorActorBase : public AActor
{
	GENERATED_BODY()

public:
	APCGExValencyEditorActorBase();

	//~ Begin AActor Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End AActor Interface

	/**
	 * Get the dirty state manager from the active Valency editor mode.
	 * @return Pointer to manager if mode is active, nullptr otherwise
	 */
	static FValencyDirtyStateManager* GetActiveDirtyStateManager();

protected:
	/**
	 * Called when a property with PCGEX_ValencyGhostRefresh metadata changes.
	 * Override in subclasses to handle ghost mesh refresh.
	 * Default: does nothing.
	 */
	virtual void OnGhostRefreshRequested() {}

	/**
	 * Called when a property with PCGEX_ValencyRebuild metadata changes (after debounce check).
	 * Override in subclasses to trigger the appropriate rebuild path.
	 * Default: does nothing.
	 */
	virtual void OnRebuildMetaTagTriggered() {}

	/**
	 * Subclass hook called after meta tag handling in PostEditChangeProperty.
	 * Override this for property-specific handling.
	 * Cage subclasses: call Super::OnPostEditChangeProperty() to get cage-base property handling.
	 */
	virtual void OnPostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) {}
};
