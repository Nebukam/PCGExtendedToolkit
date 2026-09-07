// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssemblyRoot/PCGExAssemblyRootEditorActions.h"

#include "ActorPickerMode.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Framework/Commands/UIAction.h"
#include "Modules/ModuleManager.h"
#include "PCGExAssemblyRoot.h"
#include "PCGExAssemblyRootActor.h"
#include "ScopedTransaction.h"
#include "Selection.h"
#include "Styling/AppStyle.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"

#define LOCTEXT_NAMESPACE "PCGExAssemblyRootEditor"

namespace PCGExAssemblyRootEditor
{
	const APCGExAssemblyRootActor* FindStockRoot(const AActor* Actor)
	{
		for (const AActor* Node = Actor; Node; Node = Node->GetAttachParentActor())
		{
			if (const APCGExAssemblyRootActor* Root = Cast<APCGExAssemblyRootActor>(Node))
			{
				return Root;
			}
		}
		return nullptr;
	}

	APCGExAssemblyRootActor* FindStockRoot(AActor* Actor)
	{
		return const_cast<APCGExAssemblyRootActor*>(FindStockRoot(static_cast<const AActor*>(Actor)));
	}

	bool IsAssemblyRootLike(const AActor* Actor)
	{
		return Actor && Actor->GetClass()->ImplementsInterface(UPCGExAssemblyRoot::StaticClass());
	}

	FSelectionContext GatherSelection()
	{
		TArray<AActor*> Selected;
		if (GEditor)
		{
			GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(Selected);
		}
		return GatherSelection(Selected);
	}

	FSelectionContext GatherSelection(const TArray<AActor*>& SelectedActors)
	{
		FSelectionContext Context;

		// One root in play, or none: a selected stock root counts as itself, never as content of an
		// enclosing root, so nested roots resolve to the innermost one the user actually selected.
		for (AActor* Actor : SelectedActors)
		{
			if (!Actor) { continue; }

			APCGExAssemblyRootActor* Root = FindStockRoot(Actor);
			if (!Root) { continue; }

			if (Context.Target && Context.Target != Root)
			{
				return FSelectionContext();
			}

			Context.Target = Root;
		}

		if (!Context.Target)
		{
			return Context;
		}

		for (AActor* Actor : SelectedActors)
		{
			if (!Actor || Actor == Context.Target) { continue; }

			if (!Cast<APCGExAssemblyRootActor>(Actor) && FindStockRoot(Actor) == Context.Target)
			{
				Context.Content.Add(Actor);
			}
			else if (CanAttach(Context.Target, Actor))
			{
				Context.Attachable.Add(Actor);
			}
		}

		return Context;
	}

	bool CanAttach(const APCGExAssemblyRootActor* Root, const AActor* Actor, FText* OutReason)
	{
		if (!Root || !Actor || !GEditor)
		{
			return false;
		}

		if (Actor == Root)
		{
			if (OutReason) { *OutReason = LOCTEXT("AttachSelf", "Cannot attach the assembly root to itself."); }
			return false;
		}

		if (IsAssemblyRootLike(Actor))
		{
			if (OutReason) { *OutReason = LOCTEXT("AttachRoot", "Assembly roots are not attached by the quick actions."); }
			return false;
		}

		if (FindStockRoot(Actor) == Root)
		{
			if (OutReason) { *OutReason = LOCTEXT("AttachAlreadyContent", "Already part of this assembly."); }
			return false;
		}

		if (Root->IsAttachedTo(Actor))
		{
			if (OutReason) { *OutReason = LOCTEXT("AttachAncestor", "The assembly root is attached under this actor."); }
			return false;
		}

		return GEditor->CanParentActors(Root, Actor, OutReason);
	}

	int32 AttachActors(APCGExAssemblyRootActor* Root, const TArray<AActor*>& Actors)
	{
		if (!Root || !GEditor)
		{
			return 0;
		}

		int32 Attached = 0;
		{
			const FScopedTransaction Transaction(LOCTEXT("AttachToAssembly", "Attach to Assembly Root"));
			for (AActor* Actor : Actors)
			{
				if (!CanAttach(Root, Actor)) { continue; }
				// ParentActors handles Modify on both ends, KeepWorld re-parenting and the attach broadcast.
				GEditor->ParentActors(Root, Actor, NAME_None);
				Attached++;
			}
		}

		if (Attached > 0)
		{
			// The assembly grew; the unit is the thing to have in hand now.
			GEditor->SelectNone(/*bNoteSelectionChange*/ false, /*bDeselectBSPSurfs*/ true);

			// A nested root is only selectable while its enclosing roots are latched, and the clear above
			// may have settled them off. Latch between clear and select; the tracker re-settles to the
			// same answer once the root is selected.
			for (AActor* Node = Root->GetAttachParentActor(); Node; Node = Node->GetAttachParentActor())
			{
				if (APCGExAssemblyRootActor* Enclosing = Cast<APCGExAssemblyRootActor>(Node))
				{
					Enclosing->SetEditorSubSelectionLatch(true);
				}
			}

			GEditor->SelectActor(Root, /*bInSelected*/ true, /*bNotify*/ true);
		}

		return Attached;
	}

	int32 DetachActors(const TArray<AActor*>& Actors)
	{
		if (!GEngine)
		{
			return 0;
		}

		int32 Detached = 0;
		const FScopedTransaction Transaction(LOCTEXT("DetachFromAssembly", "Detach from Assembly Root"));
		for (AActor* Actor : Actors)
		{
			AActor* Parent = Actor ? Actor->GetAttachParentActor() : nullptr;
			if (!Parent) { continue; }

			// Same shape as UEditorEngine::ParentActors' detach half: attachment lives on the child, so
			// the old parent is modified for undo without dirtying its package.
			Actor->Modify();
			Parent->Modify(/*bAlwaysMarkDirty=*/false);
			Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			GEngine->BroadcastLevelActorDetached(Actor, Parent);
			Detached++;
		}

		return Detached;
	}

	void BeginPickAndAttach(APCGExAssemblyRootActor* Root)
	{
		if (!Root)
		{
			return;
		}

		FActorPickerModeModule& Picker = FModuleManager::LoadModuleChecked<FActorPickerModeModule>("ActorPickerMode");
		const TWeakObjectPtr<APCGExAssemblyRootActor> WeakRoot(Root);

		// FOnShouldFilterActor returns true to KEEP the actor (FEdModeActorPicker::IsActorValid).
		Picker.BeginActorPickingMode(
			FOnGetAllowedClasses(),
			FOnShouldFilterActor::CreateLambda([WeakRoot](const AActor* Candidate)
			{
				return CanAttach(WeakRoot.Get(), Candidate);
			}),
			FOnActorSelected::CreateLambda([WeakRoot](AActor* Picked)
			{
				if (APCGExAssemblyRootActor* LiveRoot = WeakRoot.Get())
				{
					AttachActors(LiveRoot, {Picked});
				}
			}));
	}

#pragma region Action table

	FText GetActionLabel(const EAction Action)
	{
		switch (Action)
		{
		case EAction::PickAndAttach: return LOCTEXT("PickAndAttachLabel", "Pick & Attach");
		case EAction::AttachSelection: return LOCTEXT("AttachSelectionLabel", "Attach Selection");
		case EAction::DetachSelection: return LOCTEXT("DetachSelectionLabel", "Detach");
		default: checkNoEntry(); return FText::GetEmpty();
		}
	}

	FText GetActionTooltip(const EAction Action)
	{
		switch (Action)
		{
		case EAction::PickAndAttach:
			return LOCTEXT("PickAndAttachTooltip", "Pick an actor in the viewport and attach it to this assembly root, keeping its world transform.\nOther assembly roots cannot be picked.");
		case EAction::AttachSelection:
			return LOCTEXT("AttachSelectionTooltip", "Attach the other selected actors to this assembly root, keeping their world transforms.\nOther assembly roots and actors already inside are skipped.");
		case EAction::DetachSelection:
			return LOCTEXT("DetachSelectionTooltip", "Detach the selected content from the assembly, leaving it exactly where it stands.");
		default: checkNoEntry(); return FText::GetEmpty();
		}
	}

	FSlateIcon GetActionIcon(const EAction Action)
	{
		switch (Action)
		{
		case EAction::PickAndAttach: return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.EyeDropper");
		case EAction::AttachSelection: return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Actors.Attach");
		case EAction::DetachSelection: return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Actors.Detach");
		default: checkNoEntry(); return FSlateIcon();
		}
	}

	bool IsDestructiveAction(const EAction Action)
	{
		return Action == EAction::DetachSelection;
	}

	bool CanExecuteAction(const EAction Action, const FSelectionContext& Context)
	{
		switch (Action)
		{
		case EAction::PickAndAttach: return Context.CanPick();
		case EAction::AttachSelection: return Context.CanAttachSelection();
		case EAction::DetachSelection: return Context.CanDetachSelection();
		default: checkNoEntry(); return false;
		}
	}

	void ExecuteAction(const EAction Action, const FSelectionContext& Context)
	{
		if (!CanExecuteAction(Action, Context))
		{
			return;
		}

		switch (Action)
		{
		case EAction::PickAndAttach: BeginPickAndAttach(Context.Target);
			break;
		case EAction::AttachSelection: AttachActors(Context.Target, Context.Attachable);
			break;
		case EAction::DetachSelection: DetachActors(Context.Content);
			break;
		default: checkNoEntry();
			break;
		}
	}

#pragma endregion

	void ExtendActorContextMenu(UToolMenu* Menu)
	{
		if (!Menu || !GatherSelection().IsRelevant())
		{
			return;
		}

		FToolMenuSection& Section = Menu->AddSection("PCGExAssemblyRoot", LOCTEXT("AssemblyRootHeading", "Assembly Root"));

		auto AddAction = [&Section](const FName EntryName, const EAction Action)
		{
			// Re-gathered at execute time: the context menu can outlive the selection it was built for.
			Section.AddMenuEntry(
				EntryName,
				GetActionLabel(Action),
				GetActionTooltip(Action),
				GetActionIcon(Action),
				FUIAction(
					FExecuteAction::CreateLambda([Action]() { ExecuteAction(Action, GatherSelection()); }),
					FCanExecuteAction::CreateLambda([Action]() { return CanExecuteAction(Action, GatherSelection()); })));
		};

		AddAction("PickAndAttach", EAction::PickAndAttach);
		AddAction("AttachSelection", EAction::AttachSelection);
		AddAction("DetachSelection", EAction::DetachSelection);
	}
}

#undef LOCTEXT_NAMESPACE
