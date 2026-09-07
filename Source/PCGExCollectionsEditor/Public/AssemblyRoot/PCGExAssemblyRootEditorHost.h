// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AActor;
class APCGExAssemblyRootActor;
class SLevelViewport;
class SWidget;
class USelection;
class UTypedElementSelectionSet;

/**
 * Editor-side owner of the stock assembly root's select-as-unit behaviour and its viewport action bar.
 * One per editor module; no editor mode involved.
 *
 * Latch contract (APCGExAssemblyRootActor::SetEditorSubSelectionLatch): the typed selection set fires
 * OnPreChange BEFORE each add/remove, and a plain click clears then selects. Latching every root that is
 * selected, or has selected content, at pre-change lets the follow-up select land on the clicked child
 * instead of bouncing back to the root. OnChanged then settles the latch to "has selected content" and
 * re-pushes the subtree's selection proxies, so highlight always reflects the settled state rather than
 * whatever value the engine read mid-change.
 */
class PCGEXCOLLECTIONSEDITOR_API FPCGExAssemblyRootEditorHost final
{
public:
	FPCGExAssemblyRootEditorHost() = default;
	~FPCGExAssemblyRootEditorHost();

	/** Safe before GEngine exists: engine-bound hooks wait for PostEngineInit. */
	void Startup();
	void Shutdown();

private:
	void BindEngineHooks();
	void BindSelectionSet(UTypedElementSelectionSet* SelectionSet);
	void UnbindSelectionSet();
	void OnSelectionSetPtrChanged(USelection* Selection, UTypedElementSelectionSet* OldSet, UTypedElementSelectionSet* NewSet);

	void OnSelectionPreChange(const UTypedElementSelectionSet* SelectionSet);
	void OnSelectionChanged(const UTypedElementSelectionSet* SelectionSet);
	void OnLevelActorAttachment(AActor* Actor, const AActor* Parent);
	void OnPostUndoRedo();
	void OnMapChange(uint32 MapChangeFlags);
	void OnPIEEvent(bool bIsSimulating);

	/** Re-settles latches and the overlay from the bound set's current selection. */
	void Refresh();
	/** Settles latches from a selection and re-pushes proxies for every root touched. */
	void SettleLatches(const TArray<AActor*>& SelectedActors);
	/** Unlatches every tracked root that still exists and forgets them all. */
	void ClearLatches();
	static void PushSubtreeSelection(APCGExAssemblyRootActor* Root);
	static void GatherSelectedActors(const UTypedElementSelectionSet* SelectionSet, TArray<AActor*>& OutActors);

	void UpdateOverlay(const TArray<AActor*>& SelectedActors);
	void RemoveOverlay();

	TWeakObjectPtr<UTypedElementSelectionSet> BoundSelectionSet;
	FDelegateHandle PreChangeHandle;
	FDelegateHandle ChangedHandle;
	FDelegateHandle SelectionSetPtrChangedHandle;
	FDelegateHandle ActorAttachedHandle;
	FDelegateHandle ActorDetachedHandle;
	FDelegateHandle PostUndoRedoHandle;
	FDelegateHandle MapChangeHandle;
	FDelegateHandle PostPIEStartedHandle;
	FDelegateHandle EndPIEHandle;
	FDelegateHandle ShutdownPIEHandle;
	FDelegateHandle PostEngineInitHandle;

	/** Roots currently latched. Weak: a deleted root simply drops out. */
	TSet<TWeakObjectPtr<APCGExAssemblyRootActor>> LatchedRoots;

	/** Frame the pre-change pass last ran in: one pass per change batch, or select-all goes quadratic. */
	uint64 PreChangeHandledFrame = MAX_uint64;

	TSharedPtr<SWidget> Overlay;
	TWeakPtr<SLevelViewport> OverlayViewport;
};
