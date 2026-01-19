// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "EditorMode/PCGExValencyCageEditorMode.h"

#include "EditorModeManager.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Selection.h"
#include "LevelEditorViewport.h"

#include "EditorMode/PCGExValencyDrawHelper.h"
#include "Cages/PCGExValencyCageBase.h"
#include "Cages/PCGExValencyCage.h"
#include "Volumes/ValencyContextVolume.h"

const FEditorModeID FPCGExValencyCageEditorMode::ModeID = TEXT("PCGExValencyCageEditorMode");

FPCGExValencyCageEditorMode::FPCGExValencyCageEditorMode()
{
}

FPCGExValencyCageEditorMode::~FPCGExValencyCageEditorMode()
{
}

void FPCGExValencyCageEditorMode::Enter()
{
	FEdMode::Enter();

	// Bind to actor add/delete events to keep cache up to date
	if (GEditor)
	{
		OnActorAddedHandle = GEditor->OnLevelActorAdded().AddRaw(this, &FPCGExValencyCageEditorMode::OnLevelActorAdded);
		OnActorDeletedHandle = GEditor->OnLevelActorDeleted().AddRaw(this, &FPCGExValencyCageEditorMode::OnLevelActorDeleted);

		// Bind to selection changes for asset tracking
		OnSelectionChangedHandle = GEditor->GetSelectedActors()->SelectionChangedEvent.AddLambda([this](UObject*)
		{
			OnSelectionChanged();
		});
	}

	// Collect and fully initialize all cages
	CollectCagesFromLevel();
	CollectVolumesFromLevel();
	RefreshAllCages();

	// Initialize asset tracker with our cache references
	AssetTracker.Initialize(CachedCages, CachedVolumes);

	bCacheDirty = false;
}

void FPCGExValencyCageEditorMode::Exit()
{
	// Unbind actor add/delete events
	if (GEditor)
	{
		GEditor->OnLevelActorAdded().Remove(OnActorAddedHandle);
		GEditor->OnLevelActorDeleted().Remove(OnActorDeletedHandle);
		GEditor->GetSelectedActors()->SelectionChangedEvent.Remove(OnSelectionChangedHandle);
	}
	OnActorAddedHandle.Reset();
	OnActorDeletedHandle.Reset();
	OnSelectionChangedHandle.Reset();

	// Clear tracking state
	AssetTracker.Reset();

	CachedCages.Empty();
	CachedVolumes.Empty();

	FEdMode::Exit();
}

void FPCGExValencyCageEditorMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	FEdMode::Render(View, Viewport, PDI);

	if (!PDI)
	{
		return;
	}

	// Refresh cache if needed
	if (bCacheDirty)
	{
		CollectCagesFromLevel();
		CollectVolumesFromLevel();
		RefreshAllCages();
		bCacheDirty = false;
	}

	// Draw volumes first (background)
	for (const TWeakObjectPtr<AValencyContextVolume>& VolumePtr : CachedVolumes)
	{
		if (AValencyContextVolume* Volume = VolumePtr.Get())
		{
			FPCGExValencyDrawHelper::DrawVolume(PDI, Volume);
		}
	}

	// Draw cages
	for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : CachedCages)
	{
		if (APCGExValencyCageBase* Cage = CagePtr.Get())
		{
			FPCGExValencyDrawHelper::DrawCage(PDI, Cage);
		}
	}
}

void FPCGExValencyCageEditorMode::DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	FEdMode::DrawHUD(ViewportClient, Viewport, View, Canvas);

	if (!Canvas || !View)
	{
		return;
	}

	// Determine which cages are selected for label color
	TSet<const APCGExValencyCageBase*> SelectedCages;
	if (GEditor)
	{
		USelection* Selection = GEditor->GetSelectedActors();
		for (FSelectionIterator It(*Selection); It; ++It)
		{
			if (const APCGExValencyCageBase* SelectedCage = Cast<APCGExValencyCageBase>(*It))
			{
				SelectedCages.Add(SelectedCage);
			}
		}
	}

	// Draw labels for all cages
	for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : CachedCages)
	{
		if (const APCGExValencyCageBase* Cage = CagePtr.Get())
		{
			const bool bIsSelected = SelectedCages.Contains(Cage);
			FPCGExValencyDrawHelper::DrawCageLabels(Canvas, View, Cage, bIsSelected);
		}
	}
}

bool FPCGExValencyCageEditorMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	// TODO: Phase 2 - implement cage placement on click
	return FEdMode::HandleClick(InViewportClient, HitProxy, Click);
}

bool FPCGExValencyCageEditorMode::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if (Event == IE_Pressed)
	{
		// Ctrl+Shift+C: Cleanup stale manual connections
		if (Key == EKeys::C && ViewportClient->IsCtrlPressed() && ViewportClient->IsShiftPressed())
		{
			const int32 RemovedCount = CleanupAllManualConnections();
			if (RemovedCount > 0)
			{
				UE_LOG(LogTemp, Log, TEXT("Valency: Cleaned up %d stale manual connection(s)"), RemovedCount);
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("Valency: No stale manual connections found"));
			}
			return true;
		}
	}

	return FEdMode::InputKey(ViewportClient, Viewport, Key, Event);
}

bool FPCGExValencyCageEditorMode::IsSelectionAllowed(AActor* InActor, bool bInSelection) const
{
	// Allow selection of all actors
	return true;
}

void FPCGExValencyCageEditorMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	FEdMode::Tick(ViewportClient, DeltaTime);

	// Update asset tracking if enabled
	if (AssetTracker.IsEnabled())
	{
		TSet<APCGExValencyCage*> AffectedCages;
		if (AssetTracker.Update(AffectedCages))
		{
			AssetTracker.TriggerAutoRebuild(AffectedCages);
			RedrawViewports();
		}
	}
}

void FPCGExValencyCageEditorMode::CollectCagesFromLevel()
{
	CachedCages.Empty();

	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<APCGExValencyCageBase> It(World); It; ++It)
		{
			CachedCages.Add(*It);
		}
	}
}

void FPCGExValencyCageEditorMode::CollectVolumesFromLevel()
{
	CachedVolumes.Empty();

	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AValencyContextVolume> It(World); It; ++It)
		{
			CachedVolumes.Add(*It);
		}
	}
}

void FPCGExValencyCageEditorMode::RefreshAllCages()
{
	// Phase 1: Initialize all cages (orbitals setup, volume assignment)
	// This must happen before connection detection since connections depend on orbitals
	for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : CachedCages)
	{
		if (APCGExValencyCageBase* Cage = CagePtr.Get())
		{
			// Skip null cages - they don't have orbitals
			if (Cage->IsNullCage())
			{
				continue;
			}

			// Refresh containing volumes (transient, not saved)
			Cage->RefreshContainingVolumes();

			// Always reinitialize orbitals to ensure clean state
			Cage->InitializeOrbitalsFromSet();
		}
	}

	// Phase 2: Detect connections for all cages
	// Done as a separate pass so all cages have their orbitals ready
	for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : CachedCages)
	{
		if (APCGExValencyCageBase* Cage = CagePtr.Get())
		{
			// Skip null cages - they don't detect connections
			if (Cage->IsNullCage())
			{
				continue;
			}

			Cage->DetectNearbyConnections();
		}
	}

	// Phase 3: Refresh mirror ghost meshes for all regular cages
	// Done after all cages are initialized so source cage content is available
	for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : CachedCages)
	{
		if (APCGExValencyCage* Cage = Cast<APCGExValencyCage>(CagePtr.Get()))
		{
			Cage->RefreshMirrorGhostMeshes();
		}
	}
}

void FPCGExValencyCageEditorMode::InitializeCage(APCGExValencyCageBase* Cage)
{
	if (!Cage || Cage->IsNullCage())
	{
		return;
	}

	// Setup volumes and orbitals
	Cage->RefreshContainingVolumes();
	Cage->InitializeOrbitalsFromSet();

	// Detect connections
	Cage->DetectNearbyConnections();

	// Refresh mirror ghost meshes if this is a regular cage
	if (APCGExValencyCage* RegularCage = Cast<APCGExValencyCage>(Cage))
	{
		RegularCage->RefreshMirrorGhostMeshes();
	}
}

void FPCGExValencyCageEditorMode::OnLevelActorAdded(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	// Check if it's a cage
	if (APCGExValencyCageBase* Cage = Cast<APCGExValencyCageBase>(Actor))
	{
		// Add to cache and initialize
		CachedCages.Add(Cage);
		InitializeCage(Cage);

		// New cage might be a connection target for existing cages - refresh all connections
		for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : CachedCages)
		{
			if (APCGExValencyCageBase* OtherCage = CagePtr.Get())
			{
				if (OtherCage != Cage)
				{
					OtherCage->DetectNearbyConnections();
				}
			}
		}
	}
	// Check if it's a volume
	else if (AValencyContextVolume* Volume = Cast<AValencyContextVolume>(Actor))
	{
		CachedVolumes.Add(Volume);
		bCacheDirty = true; // Volumes affect cage orbital sets - full refresh needed
	}
}

void FPCGExValencyCageEditorMode::OnLevelActorDeleted(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	// Check if it's a cage being deleted
	if (APCGExValencyCageBase* Cage = Cast<APCGExValencyCageBase>(Actor))
	{
		// Remove from cache
		CachedCages.RemoveAll([Cage](const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr)
		{
			return CagePtr.Get() == Cage || !CagePtr.IsValid();
		});

		// Refresh connections on remaining cages
		for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : CachedCages)
		{
			if (APCGExValencyCageBase* OtherCage = CagePtr.Get())
			{
				OtherCage->DetectNearbyConnections();
			}
		}
	}
	// Check if it's a volume
	else if (AValencyContextVolume* Volume = Cast<AValencyContextVolume>(Actor))
	{
		CachedVolumes.RemoveAll([Volume](const TWeakObjectPtr<AValencyContextVolume>& VolumePtr)
		{
			return VolumePtr.Get() == Volume || !VolumePtr.IsValid();
		});
		bCacheDirty = true; // Volumes affect cage orbital sets - full refresh needed
	}
	// Check if it's a tracked asset actor being deleted
	else if (AssetTracker.OnActorDeleted(Actor))
	{
		RedrawViewports();
	}
}

void FPCGExValencyCageEditorMode::OnSelectionChanged()
{
	AssetTracker.OnSelectionChanged();

	// Immediately check containment for newly selected actors
	if (AssetTracker.IsEnabled() && AssetTracker.GetTrackedActorCount() > 0)
	{
		TSet<APCGExValencyCage*> AffectedCages;
		if (AssetTracker.Update(AffectedCages))
		{
			AssetTracker.TriggerAutoRebuild(AffectedCages);
			RedrawViewports();
		}
	}
}

void FPCGExValencyCageEditorMode::SetAllCageDebugComponentsVisible(bool bVisible)
{
	if (CachedCages.Num() == 0)
	{
		CollectCagesFromLevel();
	}

	for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : CachedCages)
	{
		if (APCGExValencyCageBase* Cage = CagePtr.Get())
		{
			Cage->SetDebugComponentsVisible(bVisible);
		}
	}
}

int32 FPCGExValencyCageEditorMode::CleanupAllManualConnections()
{
	int32 TotalRemoved = 0;

	for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : CachedCages)
	{
		if (APCGExValencyCageBase* Cage = CagePtr.Get())
		{
			TotalRemoved += Cage->CleanupManualConnections();
		}
	}

	if (TotalRemoved > 0)
	{
		RedrawViewports();
	}

	return TotalRemoved;
}

void FPCGExValencyCageEditorMode::RedrawViewports()
{
	if (GEditor)
	{
		GEditor->RedrawAllViewports();

		for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
		{
			if (ViewportClient)
			{
				ViewportClient->Invalidate();
			}
		}
	}
}
