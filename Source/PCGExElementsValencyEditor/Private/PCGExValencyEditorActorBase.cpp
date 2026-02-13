// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExValencyEditorActorBase.h"

#include "PCGExValencyEditorSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "EditorMode/PCGExValencyCageEditorMode.h"
#include "EditorMode/PCGExValencyDirtyState.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorModeManager.h"
#endif

APCGExValencyEditorActorBase::APCGExValencyEditorActorBase()
{
}

void APCGExValencyEditorActorBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	LastKnownTransform = GetActorTransform();
}

void APCGExValencyEditorActorBase::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	if (!bFinished)
	{
		if (!bIsDraggingTracking)
		{
			// First frame of drag - check if CTRL is held
			bIsDraggingTracking = true;

			if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetModifierKeys().IsControlDown())
			{
				BeginDragContainedAssets();
			}
		}

		if (bIsDraggingAssets)
		{
			UpdateDraggedActorPositions();
		}
	}
	else
	{
		if (bIsDraggingAssets)
		{
			UpdateDraggedActorPositions();
			EndDragContainedAssets();
		}

		bIsDraggingTracking = false;
		LastKnownTransform = GetActorTransform();
	}
}

void APCGExValencyEditorActorBase::BeginDragContainedAssets()
{
	DraggedActors.Reset();

	TArray<AActor*> Actors;
	CollectDraggableActors(Actors);

	if (Actors.IsEmpty())
	{
		return;
	}

	// Use LastKnownTransform (from before the drag) to compute correct relative offsets.
	// By the time PostEditMove is called, this actor has already been moved by the editor,
	// but the contained actors are still at their original positions.
	const FTransform PreDragTransform = LastKnownTransform;

	TSet<AActor*> SeenActors;
	for (AActor* Actor : Actors)
	{
		if (!Actor || Actor == this || Actor->IsActorBeingDestroyed())
		{
			continue;
		}

		bool bAlreadyInSet = false;
		SeenActors.Add(Actor, &bAlreadyInSet);
		if (bAlreadyInSet)
		{
			continue;
		}

		FDraggedActorInfo Info;
		Info.Actor = Actor;
		Info.RelativeTransform = Actor->GetActorTransform().GetRelativeTransform(PreDragTransform);
		DraggedActors.Add(Info);
	}

	if (DraggedActors.IsEmpty())
	{
		return;
	}

	DragAssetTransaction = MakeUnique<FScopedTransaction>(NSLOCTEXT("PCGExValency", "MoveCageWithAssets", "Move Cage With Contained Assets"));

	Modify();
	for (const FDraggedActorInfo& Info : DraggedActors)
	{
		if (AActor* Actor = Info.Actor.Get())
		{
			Actor->Modify();
		}
	}

	bIsDraggingAssets = true;
}

void APCGExValencyEditorActorBase::UpdateDraggedActorPositions()
{
	const FTransform CurrentTransform = GetActorTransform();

	for (const FDraggedActorInfo& Info : DraggedActors)
	{
		AActor* Actor = Info.Actor.Get();
		if (!Actor)
		{
			continue;
		}

		const FTransform NewTransform = Info.RelativeTransform * CurrentTransform;
		Actor->SetActorTransform(NewTransform);
	}
}

void APCGExValencyEditorActorBase::EndDragContainedAssets()
{
	bIsDraggingAssets = false;
	DraggedActors.Reset();
	DragAssetTransaction.Reset();
}

void APCGExValencyEditorActorBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// ========== Meta Tag Handling ==========

	// Check PCGEX_ValencyGhostRefresh meta on any property in the change chain
	{
		bool bGhostRefresh = false;
		if (const FProperty* Property = PropertyChangedEvent.Property)
		{
			bGhostRefresh = Property->HasMetaData(TEXT("PCGEX_ValencyGhostRefresh"));
		}
		if (!bGhostRefresh && PropertyChangedEvent.MemberProperty)
		{
			bGhostRefresh = PropertyChangedEvent.MemberProperty->HasMetaData(TEXT("PCGEX_ValencyGhostRefresh"));
		}

		if (bGhostRefresh)
		{
			OnGhostRefreshRequested();
		}
	}

	// Check PCGEX_ValencyRebuild meta on any property in the change chain
	{
		bool bShouldRebuild = false;
		if (const FProperty* Property = PropertyChangedEvent.Property)
		{
			if (Property->HasMetaData(TEXT("PCGEX_ValencyRebuild")))
			{
				bShouldRebuild = true;
			}
		}
		if (!bShouldRebuild && PropertyChangedEvent.MemberProperty)
		{
			if (PropertyChangedEvent.MemberProperty->HasMetaData(TEXT("PCGEX_ValencyRebuild")))
			{
				bShouldRebuild = true;
			}
		}

		// Debounce interactive changes (dragging sliders) to prevent spam
		if (bShouldRebuild && !UPCGExValencyEditorSettings::ShouldAllowRebuild(PropertyChangedEvent.ChangeType))
		{
			bShouldRebuild = false;
		}

		if (bShouldRebuild)
		{
			OnRebuildMetaTagTriggered();
		}
	}

	// Subclass hook for class-specific property handling
	OnPostEditChangeProperty(PropertyChangedEvent);
}

FValencyDirtyStateManager* APCGExValencyEditorActorBase::GetActiveDirtyStateManager()
{
#if WITH_EDITOR
	if (GEditor)
	{
		if (GLevelEditorModeTools().IsModeActive(UPCGExValencyCageEditorMode::ModeID))
		{
			if (UPCGExValencyCageEditorMode* Mode = Cast<UPCGExValencyCageEditorMode>(
				GLevelEditorModeTools().GetActiveScriptableMode(UPCGExValencyCageEditorMode::ModeID)))
			{
				return &Mode->GetDirtyStateManager();
			}
		}
	}
#endif
	return nullptr;
}
