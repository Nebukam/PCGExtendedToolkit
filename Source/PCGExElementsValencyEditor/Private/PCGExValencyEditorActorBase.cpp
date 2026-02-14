// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExValencyEditorActorBase.h"

#include "PCGExValencyEditorSettings.h"
#include "EditorMode/PCGExValencyCageEditorMode.h"
#include "EditorMode/PCGExValencyDirtyState.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorModeManager.h"
#endif

APCGExValencyEditorActorBase::APCGExValencyEditorActorBase()
{
}

void APCGExValencyEditorActorBase::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	// Clean up drag state when the editor drag operation ends
	if (bFinished && bIsDraggingAssets)
	{
		EndDragContainedAssets();
	}
}

void APCGExValencyEditorActorBase::EditorApplyTranslation(const FVector& DeltaTranslation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	Super::EditorApplyTranslation(DeltaTranslation, bAltDown, bShiftDown, bCtrlDown);

	if (bCtrlDown)
	{
		if (!bIsDraggingAssets)
		{
			BeginDragContainedAssets();
		}

		// Apply the exact same translation delta — zero lag
		for (const TWeakObjectPtr<AActor>& WeakActor : DraggedActors)
		{
			if (AActor* Actor = WeakActor.Get())
			{
				Actor->AddActorWorldOffset(DeltaTranslation);
			}
		}
	}
	else if (bIsDraggingAssets)
	{
		EndDragContainedAssets();
	}
}

void APCGExValencyEditorActorBase::EditorApplyRotation(const FRotator& DeltaRotation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	Super::EditorApplyRotation(DeltaRotation, bAltDown, bShiftDown, bCtrlDown);

	if (bCtrlDown)
	{
		if (!bIsDraggingAssets)
		{
			BeginDragContainedAssets();
		}

		// Rotate contained actors around the cage's pivot
		const FVector CageLocation = GetActorLocation();
		const FQuat DeltaQuat = DeltaRotation.Quaternion();

		for (const TWeakObjectPtr<AActor>& WeakActor : DraggedActors)
		{
			if (AActor* Actor = WeakActor.Get())
			{
				// Orbit position around the cage
				const FVector Offset = Actor->GetActorLocation() - CageLocation;
				Actor->SetActorLocation(CageLocation + DeltaQuat.RotateVector(Offset));

				// Rotate orientation
				Actor->SetActorRotation((DeltaQuat * Actor->GetActorQuat()).Rotator());
			}
		}
	}
	else if (bIsDraggingAssets)
	{
		EndDragContainedAssets();
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

		DraggedActors.Add(Actor);
	}

	if (DraggedActors.IsEmpty())
	{
		return;
	}

	// Snapshot contained actors into the editor's already-active gizmo drag transaction.
	// No separate FScopedTransaction — that conflicts with the editor's transaction on release.
	for (const TWeakObjectPtr<AActor>& WeakActor : DraggedActors)
	{
		if (AActor* Actor = WeakActor.Get())
		{
			Actor->Modify();
		}
	}

	bIsDraggingAssets = true;
}

void APCGExValencyEditorActorBase::EndDragContainedAssets()
{
	bIsDraggingAssets = false;
	DraggedActors.Reset();
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
