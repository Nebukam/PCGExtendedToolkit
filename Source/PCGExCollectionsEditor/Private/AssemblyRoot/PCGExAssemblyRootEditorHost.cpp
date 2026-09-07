// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssemblyRoot/PCGExAssemblyRootEditorHost.h"

#include "AssemblyRoot/PCGExAssemblyRootEditorActions.h"
#include "AssemblyRoot/SPCGExAssemblyRootBar.h"
#include "Editor.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Engine/Engine.h"
#include "LevelEditor.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "PCGExAssemblyRootActor.h"
#include "SLevelViewport.h"
#include "Selection.h"

FPCGExAssemblyRootEditorHost::~FPCGExAssemblyRootEditorHost()
{
	Shutdown();
}

void FPCGExAssemblyRootEditorHost::Startup()
{
	// Static delegates are safe at module startup; everything reached through GEngine/GEditor is not.
	SelectionSetPtrChangedHandle = USelection::SelectionElementSelectionPtrChanged.AddRaw(this, &FPCGExAssemblyRootEditorHost::OnSelectionSetPtrChanged);
	PostUndoRedoHandle = FEditorDelegates::PostUndoRedo.AddRaw(this, &FPCGExAssemblyRootEditorHost::OnPostUndoRedo);
	MapChangeHandle = FEditorDelegates::MapChange.AddRaw(this, &FPCGExAssemblyRootEditorHost::OnMapChange);
	PostPIEStartedHandle = FEditorDelegates::PostPIEStarted.AddRaw(this, &FPCGExAssemblyRootEditorHost::OnPIEEvent);
	EndPIEHandle = FEditorDelegates::EndPIE.AddRaw(this, &FPCGExAssemblyRootEditorHost::OnPIEEvent);
	ShutdownPIEHandle = FEditorDelegates::ShutdownPIE.AddRaw(this, &FPCGExAssemblyRootEditorHost::OnPIEEvent);

	if (GEngine)
	{
		BindEngineHooks();
	}
	else
	{
		PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(this, &FPCGExAssemblyRootEditorHost::BindEngineHooks);
	}
}

void FPCGExAssemblyRootEditorHost::Shutdown()
{
	USelection::SelectionElementSelectionPtrChanged.Remove(SelectionSetPtrChangedHandle);
	FEditorDelegates::PostUndoRedo.Remove(PostUndoRedoHandle);
	FEditorDelegates::MapChange.Remove(MapChangeHandle);
	FEditorDelegates::PostPIEStarted.Remove(PostPIEStartedHandle);
	FEditorDelegates::EndPIE.Remove(EndPIEHandle);
	FEditorDelegates::ShutdownPIE.Remove(ShutdownPIEHandle);
	FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);

	if (GEngine)
	{
		GEngine->OnLevelActorAttached().Remove(ActorAttachedHandle);
		GEngine->OnLevelActorDetached().Remove(ActorDetachedHandle);
	}

	UnbindSelectionSet();
	RemoveOverlay();
	ClearLatches();
}

void FPCGExAssemblyRootEditorHost::ClearLatches()
{
	for (const TWeakObjectPtr<APCGExAssemblyRootActor>& WeakRoot : LatchedRoots)
	{
		if (APCGExAssemblyRootActor* Root = WeakRoot.Get())
		{
			Root->SetEditorSubSelectionLatch(false);
		}
	}
	LatchedRoots.Empty();
}

void FPCGExAssemblyRootEditorHost::BindEngineHooks()
{
	if (!GEngine)
	{
		return;
	}

	// Outliner drag-drop attaches and detaches move content without touching the selection.
	ActorAttachedHandle = GEngine->OnLevelActorAttached().AddRaw(this, &FPCGExAssemblyRootEditorHost::OnLevelActorAttachment);
	ActorDetachedHandle = GEngine->OnLevelActorDetached().AddRaw(this, &FPCGExAssemblyRootEditorHost::OnLevelActorAttachment);

	if (GEditor)
	{
		BindSelectionSet(GEditor->GetSelectedActors()->GetElementSelectionSet());
	}
}

#pragma region Selection set binding

void FPCGExAssemblyRootEditorHost::BindSelectionSet(UTypedElementSelectionSet* SelectionSet)
{
	UnbindSelectionSet();
	if (!SelectionSet)
	{
		return;
	}

	BoundSelectionSet = SelectionSet;
	PreChangeHandle = SelectionSet->OnPreChange().AddRaw(this, &FPCGExAssemblyRootEditorHost::OnSelectionPreChange);
	ChangedHandle = SelectionSet->OnChanged().AddRaw(this, &FPCGExAssemblyRootEditorHost::OnSelectionChanged);
}

void FPCGExAssemblyRootEditorHost::UnbindSelectionSet()
{
	if (UTypedElementSelectionSet* SelectionSet = BoundSelectionSet.Get())
	{
		SelectionSet->OnPreChange().Remove(PreChangeHandle);
		SelectionSet->OnChanged().Remove(ChangedHandle);
	}
	BoundSelectionSet.Reset();
	PreChangeHandle.Reset();
	ChangedHandle.Reset();
}

void FPCGExAssemblyRootEditorHost::OnSelectionSetPtrChanged(USelection* Selection, UTypedElementSelectionSet* OldSet, UTypedElementSelectionSet* NewSet)
{
	// SLevelEditor assigns the same set to the actor AND component selections; follow the actor one only.
	if (!GEditor || Selection != GEditor->GetSelectedActors())
	{
		return;
	}

	BindSelectionSet(NewSet);
	RemoveOverlay();
}

void FPCGExAssemblyRootEditorHost::GatherSelectedActors(const UTypedElementSelectionSet* SelectionSet, TArray<AActor*>& OutActors)
{
	OutActors.Reset();
	if (SelectionSet)
	{
		OutActors = SelectionSet->GetSelectedObjects<AActor>();
	}
}

#pragma endregion

#pragma region Latch

void FPCGExAssemblyRootEditorHost::OnSelectionPreChange(const UTypedElementSelectionSet* SelectionSet)
{
	if (SelectionSet != BoundSelectionSet.Get())
	{
		return;
	}

	// Pre-change fires before EVERY add/remove of a batch; the first one sees the full previous selection
	// and that is the only one that matters. Without this guard a select-all clear walks N actors N times.
	if (PreChangeHandledFrame == GFrameCounter)
	{
		return;
	}
	PreChangeHandledFrame = GFrameCounter;

	// Set only, never clear: the follow-up select's pre-change would otherwise see an empty set.
	// Every root above a selected actor latches -- nested roots drill one level at a time.
	TArray<AActor*> Selected;
	GatherSelectedActors(SelectionSet, Selected);

	for (AActor* Actor : Selected)
	{
		for (AActor* Node = Actor; Node; Node = Node->GetAttachParentActor())
		{
			if (APCGExAssemblyRootActor* Root = Cast<APCGExAssemblyRootActor>(Node))
			{
				Root->SetEditorSubSelectionLatch(true);
				LatchedRoots.Add(Root);
			}
		}
	}
}

void FPCGExAssemblyRootEditorHost::OnSelectionChanged(const UTypedElementSelectionSet* SelectionSet)
{
	if (SelectionSet != BoundSelectionSet.Get())
	{
		return;
	}

	// Settled: the next batch, even in this same frame, must re-arm.
	PreChangeHandledFrame = MAX_uint64;

	TArray<AActor*> Selected;
	GatherSelectedActors(SelectionSet, Selected);
	SettleLatches(Selected);
	UpdateOverlay(Selected);
}

void FPCGExAssemblyRootEditorHost::Refresh()
{
	TArray<AActor*> Selected;
	GatherSelectedActors(BoundSelectionSet.Get(), Selected);
	SettleLatches(Selected);
	UpdateOverlay(Selected);
}

void FPCGExAssemblyRootEditorHost::SettleLatches(const TArray<AActor*>& SelectedActors)
{
	// Settled latch = "has selected content". A root selected on its own is NOT latched, which is what
	// keeps its whole subtree highlighting as a unit; pre-change re-arms it for the next click.
	TSet<TWeakObjectPtr<APCGExAssemblyRootActor>> Active;
	TSet<TWeakObjectPtr<APCGExAssemblyRootActor>> Touched;

	for (AActor* Actor : SelectedActors)
	{
		if (!Actor) { continue; }

		if (APCGExAssemblyRootActor* Self = Cast<APCGExAssemblyRootActor>(Actor))
		{
			Touched.Add(Self);
		}

		for (AActor* Node = Actor->GetAttachParentActor(); Node; Node = Node->GetAttachParentActor())
		{
			if (APCGExAssemblyRootActor* Root = Cast<APCGExAssemblyRootActor>(Node))
			{
				Active.Add(Root);
			}
		}
	}

	Touched.Append(LatchedRoots);
	Touched.Append(Active);

	for (const TWeakObjectPtr<APCGExAssemblyRootActor>& WeakRoot : Touched)
	{
		APCGExAssemblyRootActor* Root = WeakRoot.Get();
		if (!Root) { continue; }

		Root->SetEditorSubSelectionLatch(Active.Contains(WeakRoot));
		PushSubtreeSelection(Root);
	}

	LatchedRoots = MoveTemp(Active);
}

void FPCGExAssemblyRootEditorHost::PushSubtreeSelection(APCGExAssemblyRootActor* Root)
{
	// The engine pushed proxies mid-change, against whatever the latch read then. Push again now that
	// it is settled. AActor::PushSelectionToProxies recurses into attached actors only while the root
	// is a selection parent, so a latched root's content is walked by hand.
	Root->PushSelectionToProxies();

	if (Root->GetEditorSubSelectionLatch())
	{
		TArray<AActor*> Descendants;
		Root->GetAttachedActors(Descendants, /*bResetArray=*/true, /*bRecursivelyIncludeAttachedActors=*/true);
		for (AActor* Descendant : Descendants)
		{
			if (Descendant) { Descendant->PushSelectionToProxies(); }
		}
	}
}

#pragma endregion

#pragma region Refresh triggers

void FPCGExAssemblyRootEditorHost::OnLevelActorAttachment(AActor* Actor, const AActor* Parent)
{
	Refresh();
}

void FPCGExAssemblyRootEditorHost::OnPostUndoRedo()
{
	Refresh();
}

void FPCGExAssemblyRootEditorHost::OnPIEEvent(bool bIsSimulating)
{
	Refresh();
}

void FPCGExAssemblyRootEditorHost::OnMapChange(uint32 MapChangeFlags)
{
	// Fires for saves too, where every actor survives: unlatch, never just forget.
	RemoveOverlay();
	ClearLatches();
}

#pragma endregion

#pragma region Overlay

void FPCGExAssemblyRootEditorHost::UpdateOverlay(const TArray<AActor*>& SelectedActors)
{
	// The bar rides the active level viewport and exists only while a stock root is in play. Never
	// during PIE: the level viewport is showing the play world then.
	if (!GEditor || GEditor->IsPlayingSessionInEditor() || !PCGExAssemblyRootEditor::GatherSelection(SelectedActors).IsRelevant())
	{
		RemoveOverlay();
		return;
	}

	FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor");
	const TSharedPtr<SLevelViewport> Viewport = LevelEditor ? LevelEditor->GetFirstActiveLevelViewport() : nullptr;
	if (!Viewport.IsValid())
	{
		RemoveOverlay();
		return;
	}

	if (Overlay.IsValid() && OverlayViewport.Pin() == Viewport)
	{
		return;
	}

	RemoveOverlay();

	const TSharedRef<SPCGExAssemblyRootBar> Bar = SNew(SPCGExAssemblyRootBar);
	// The bar fills the viewport to place itself; only its buttons may take hits.
	Bar->SetVisibility(EVisibility::SelfHitTestInvisible);
	Viewport->AddOverlayWidget(Bar);

	Overlay = Bar;
	OverlayViewport = Viewport;
}

void FPCGExAssemblyRootEditorHost::RemoveOverlay()
{
	if (Overlay.IsValid())
	{
		if (const TSharedPtr<SLevelViewport> Viewport = OverlayViewport.Pin())
		{
			Viewport->RemoveOverlayWidget(Overlay.ToSharedRef());
		}
	}
	Overlay.Reset();
	OverlayViewport.Reset();
}

#pragma endregion
