// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Textures/SlateIcon.h"

class AActor;
class APCGExAssemblyRootActor;
class UToolMenu;

/**
 * Editor actions for the stock assembly root (APCGExAssemblyRootActor and its Blueprint subclasses).
 * Deliberately NOT for every IPCGExAssemblyRoot implementor: Valency cages implement the interface and
 * carry their own editor mode UI.
 */
namespace PCGExAssemblyRootEditor
{
	/** Nearest stock root: the actor itself, else its closest attach ancestor that is one. */
	PCGEXCOLLECTIONSEDITOR_API APCGExAssemblyRootActor* FindStockRoot(AActor* Actor);
	PCGEXCOLLECTIONSEDITOR_API const APCGExAssemblyRootActor* FindStockRoot(const AActor* Actor);

	/** Any IPCGExAssemblyRoot implementor -- stock root or Valency cage. */
	PCGEXCOLLECTIONSEDITOR_API bool IsAssemblyRootLike(const AActor* Actor);

	/**
	 * What the current level selection means to the actions. Target is the ONE stock root in play: the
	 * single selected root, or the shared root of selected content. Two roots, or content from two
	 * roots, leave no target and every action inert.
	 */
	struct PCGEXCOLLECTIONSEDITOR_API FSelectionContext
	{
		/** The root the actions operate on. Null: nothing applies. */
		APCGExAssemblyRootActor* Target = nullptr;
		/** Selected actors that are descendants of Target. */
		TArray<AActor*> Content;
		/** Selected actors that CanAttach to Target. */
		TArray<AActor*> Attachable;

		bool IsRelevant() const { return Target != nullptr; }
		bool CanAttachSelection() const { return Target && !Attachable.IsEmpty(); }
		bool CanDetachSelection() const { return Target && !Content.IsEmpty(); }
		bool CanPick() const { return Target != nullptr; }
	};

	/** From the editor's current actor selection. */
	PCGEXCOLLECTIONSEDITOR_API FSelectionContext GatherSelection();
	/** From an explicit actor list -- the selection tracker feeds the typed selection set's own view here. */
	PCGEXCOLLECTIONSEDITOR_API FSelectionContext GatherSelection(const TArray<AActor*>& SelectedActors);

	/** Not the root, not an assembly root of any kind, not already inside the root, and the engine agrees. */
	PCGEXCOLLECTIONSEDITOR_API bool CanAttach(const APCGExAssemblyRootActor* Root, const AActor* Actor, FText* OutReason = nullptr);

	/** Attaches every eligible actor (world transform kept) in one transaction, then selects the root. Returns how many attached. */
	PCGEXCOLLECTIONSEDITOR_API int32 AttachActors(APCGExAssemblyRootActor* Root, const TArray<AActor*>& Actors);

	/** Detaches each actor from its attach parent (world transform kept) in one transaction. Selection is left as-is. */
	PCGEXCOLLECTIONSEDITOR_API int32 DetachActors(const TArray<AActor*>& Actors);

	/** Starts the level viewport eyedropper; the picked actor is attached to Root. Filters through CanAttach. */
	PCGEXCOLLECTIONSEDITOR_API void BeginPickAndAttach(APCGExAssemblyRootActor* Root);

	/** Adds the assembly root section to an actor context menu when the selection has a target. */
	PCGEXCOLLECTIONSEDITOR_API void ExtendActorContextMenu(UToolMenu* Menu);

	/** Shared labels/tooltips/icons so the context menu and the viewport bar cannot drift apart. */
	enum class EAction : uint8
	{
		PickAndAttach,
		AttachSelection,
		DetachSelection
	};

	PCGEXCOLLECTIONSEDITOR_API FText GetActionLabel(EAction Action);
	PCGEXCOLLECTIONSEDITOR_API FText GetActionTooltip(EAction Action);
	PCGEXCOLLECTIONSEDITOR_API FSlateIcon GetActionIcon(EAction Action);
	PCGEXCOLLECTIONSEDITOR_API bool IsDestructiveAction(EAction Action);
	PCGEXCOLLECTIONSEDITOR_API bool CanExecuteAction(EAction Action, const FSelectionContext& Context);
	PCGEXCOLLECTIONSEDITOR_API void ExecuteAction(EAction Action, const FSelectionContext& Context);
}
