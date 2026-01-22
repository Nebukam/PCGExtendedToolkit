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
#include "Cages/PCGExValencyCagePattern.h"
#include "Cages/PCGExValencyAssetPalette.h"
#include "Volumes/ValencyContextVolume.h"

const FEditorModeID FPCGExValencyCageEditorMode::ModeID = TEXT("PCGExValencyCageEditorMode");

FPCGExValencyCageEditorMode::FPCGExValencyCageEditorMode()
{
}

FPCGExValencyCageEditorMode::~FPCGExValencyCageEditorMode()
{
}

FValencyReferenceTracker* FPCGExValencyCageEditorMode::GetActiveReferenceTracker()
{
	if (GLevelEditorModeTools().IsModeActive(ModeID))
	{
		if (FPCGExValencyCageEditorMode* Mode = static_cast<FPCGExValencyCageEditorMode*>(GLevelEditorModeTools().GetActiveMode(ModeID)))
		{
			return &Mode->ReferenceTracker;
		}
	}
	return nullptr;
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
	CollectPalettesFromLevel();
	RefreshAllCages();

	// Initialize asset tracker with our cache references
	AssetTracker.Initialize(CachedCages, CachedVolumes, CachedPalettes);

	// Initialize reference tracker FIRST (builds dependency graph for change propagation)
	ReferenceTracker.Initialize(&CachedCages, &CachedVolumes, &CachedPalettes);

	// Initialize dirty state manager with all cached actors and reference tracker
	DirtyStateManager.Initialize(CachedCages, CachedVolumes, CachedPalettes, &ReferenceTracker);

	// Capture current selection state - handles case where actors are already selected
	// when entering Valency mode (OnSelectionChanged won't fire for existing selection)
	OnSelectionChanged();

	bCacheDirty = false;

	// Skip first dirty process to allow system to stabilize after mode entry
	bSkipNextDirtyProcess = true;
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
	DirtyStateManager.Reset();
	ReferenceTracker.Reset();

	CachedCages.Empty();
	CachedVolumes.Empty();
	CachedPalettes.Empty();

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
		CollectPalettesFromLevel();
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

	// Draw asset palettes
	for (const TWeakObjectPtr<APCGExValencyAssetPalette>& PalettePtr : CachedPalettes)
	{
		if (APCGExValencyAssetPalette* Palette = PalettePtr.Get())
		{
			FPCGExValencyDrawHelper::DrawPalette(PDI, Palette);
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

	// Draw labels for asset palettes
	TSet<const APCGExValencyAssetPalette*> SelectedPalettes;
	if (GEditor)
	{
		USelection* Selection = GEditor->GetSelectedActors();
		for (FSelectionIterator It(*Selection); It; ++It)
		{
			if (const APCGExValencyAssetPalette* SelectedPalette = Cast<APCGExValencyAssetPalette>(*It))
			{
				SelectedPalettes.Add(SelectedPalette);
			}
		}
	}

	for (const TWeakObjectPtr<APCGExValencyAssetPalette>& PalettePtr : CachedPalettes)
	{
		if (const APCGExValencyAssetPalette* Palette = PalettePtr.Get())
		{
			const bool bIsSelected = SelectedPalettes.Contains(Palette);
			FPCGExValencyDrawHelper::DrawPaletteLabels(Canvas, View, Palette, bIsSelected);
		}
	}
}

bool FPCGExValencyCageEditorMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	// Don't consume clicks - let the standard selection system handle them
	// This ensures volumes, palettes, and other actors remain selectable
	return false;
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

	// Update asset tracking if enabled - this marks cages/palettes dirty
	if (AssetTracker.IsEnabled())
	{
		TSet<APCGExValencyCage*> AffectedCages;
		TSet<APCGExValencyAssetPalette*> AffectedPalettes;
		if (AssetTracker.Update(AffectedCages, AffectedPalettes))
		{
			// Mark affected cages dirty
			for (APCGExValencyCage* Cage : AffectedCages)
			{
				if (Cage)
				{
					DirtyStateManager.MarkCageDirty(Cage, EValencyDirtyFlags::Assets);
				}
			}
			// Mark affected palettes dirty (will cascade to mirroring cages)
			for (APCGExValencyAssetPalette* Palette : AffectedPalettes)
			{
				if (Palette)
				{
					DirtyStateManager.MarkPaletteDirty(Palette, EValencyDirtyFlags::Assets);
				}
			}
		}
	}

	// Process all dirty state once per frame (coalesced rebuilds)
	// Skip dirty processing on first frame after mode entry (system stabilization)
	// NOTE: We do NOT reset dirty state - changes are queued for the next frame
	if (bSkipNextDirtyProcess)
	{
		bSkipNextDirtyProcess = false;
		// Don't clear dirty state - let it accumulate for processing on next frame
	}
	else if (DirtyStateManager.ProcessDirty())
	{
		RedrawViewports();
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

void FPCGExValencyCageEditorMode::CollectPalettesFromLevel()
{
	CachedPalettes.Empty();

	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<APCGExValencyAssetPalette> It(World); It; ++It)
		{
			CachedPalettes.Add(*It);
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

	// Phase 3: Scan contained assets for all cages and palettes
	// Must happen before ghost mesh refresh so mirrored content is available
	for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : CachedCages)
	{
		if (APCGExValencyCage* Cage = Cast<APCGExValencyCage>(CagePtr.Get()))
		{
			if (Cage->bAutoRegisterContainedAssets)
			{
				Cage->ScanAndRegisterContainedAssets();
			}
		}
	}

	for (const TWeakObjectPtr<APCGExValencyAssetPalette>& PalettePtr : CachedPalettes)
	{
		if (APCGExValencyAssetPalette* Palette = PalettePtr.Get())
		{
			if (Palette->bAutoRegisterContainedAssets)
			{
				Palette->ScanAndRegisterContainedAssets();
			}
		}
	}

	// Phase 4: Refresh ghost meshes for all cages
	// Done after all cages have their scanned content available
	for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : CachedCages)
	{
		if (APCGExValencyCage* Cage = Cast<APCGExValencyCage>(CagePtr.Get()))
		{
			Cage->RefreshMirrorGhostMeshes();
		}
		else if (APCGExValencyCagePattern* PatternCage = Cast<APCGExValencyCagePattern>(CagePtr.Get()))
		{
			PatternCage->RefreshProxyGhostMesh();
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

	// Detect connections (uses virtual filter - pattern cages only connect to pattern cages)
	Cage->DetectNearbyConnections();

	// Scan and refresh ghost meshes
	if (APCGExValencyCage* RegularCage = Cast<APCGExValencyCage>(Cage))
	{
		if (RegularCage->bAutoRegisterContainedAssets)
		{
			RegularCage->ScanAndRegisterContainedAssets();
		}
		RegularCage->RefreshMirrorGhostMeshes();
	}
	else if (APCGExValencyCagePattern* PatternCage = Cast<APCGExValencyCagePattern>(Cage))
	{
		PatternCage->RefreshProxyGhostMesh();
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
		// (ShouldConsiderCageForConnection filter ensures pattern cages only connect to pattern cages)
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

		// Rebuild dependency graph - new cage may have MirrorSources
		ReferenceTracker.RebuildDependencyGraph();

		// CRITICAL: Mark the new cage's containing volumes dirty to trigger rebuild
		// This ensures the build includes the newly added cage
		for (const TWeakObjectPtr<AValencyContextVolume>& VolumePtr : Cage->GetContainingVolumes())
		{
			if (AValencyContextVolume* Volume = VolumePtr.Get())
			{
				DirtyStateManager.MarkVolumeDirty(Volume, EValencyDirtyFlags::Structure);
			}
		}
	}
	// Check if it's a volume
	else if (AValencyContextVolume* Volume = Cast<AValencyContextVolume>(Actor))
	{
		CachedVolumes.Add(Volume);
		bCacheDirty = true; // Volumes affect cage orbital sets - full refresh needed
	}
	// Check if it's an asset palette
	else if (APCGExValencyAssetPalette* Palette = Cast<APCGExValencyAssetPalette>(Actor))
	{
		CachedPalettes.Add(Palette);
		// Palettes can be MirrorSources - rebuild dependency graph
		ReferenceTracker.RebuildDependencyGraph();
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
		// CRITICAL: Capture containing volumes BEFORE removing from cache
		// We need to mark these dirty to trigger a rebuild without the deleted cage
		TArray<AValencyContextVolume*> AffectedVolumes;
		for (const TWeakObjectPtr<AValencyContextVolume>& VolumePtr : Cage->GetContainingVolumes())
		{
			if (AValencyContextVolume* Volume = VolumePtr.Get())
			{
				AffectedVolumes.Add(Volume);
			}
		}

		// Remove from cache
		CachedCages.RemoveAll([Cage](const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr)
		{
			return CagePtr.Get() == Cage || !CagePtr.IsValid();
		});

		// Rebuild dependency graph - removed cage may have been a dependency
		ReferenceTracker.RebuildDependencyGraph();

		// Refresh connections on remaining cages
		// (ShouldConsiderCageForConnection filter ensures pattern cages only connect to pattern cages)
		for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : CachedCages)
		{
			if (APCGExValencyCageBase* OtherCage = CagePtr.Get())
			{
				OtherCage->DetectNearbyConnections();
			}
		}

		// CRITICAL: Mark affected volumes dirty to trigger rebuild
		// This ensures the build reflects the cage removal
		for (AValencyContextVolume* Volume : AffectedVolumes)
		{
			DirtyStateManager.MarkVolumeDirty(Volume, EValencyDirtyFlags::Structure);
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
	// Check if it's an asset palette
	else if (APCGExValencyAssetPalette* Palette = Cast<APCGExValencyAssetPalette>(Actor))
	{
		CachedPalettes.RemoveAll([Palette](const TWeakObjectPtr<APCGExValencyAssetPalette>& PalettePtr)
		{
			return PalettePtr.Get() == Palette || !PalettePtr.IsValid();
		});
		// Rebuild dependency graph - removed palette may have been a dependency
		ReferenceTracker.RebuildDependencyGraph();
	}
	// Check if it's a tracked asset actor being deleted
	else
	{
		APCGExValencyCage* AffectedCage = nullptr;
		if (AssetTracker.OnActorDeleted(Actor, AffectedCage))
		{
			if (AffectedCage)
			{
				DirtyStateManager.MarkCageDirty(AffectedCage, EValencyDirtyFlags::Assets);
			}
		}
	}
}

void FPCGExValencyCageEditorMode::OnSelectionChanged()
{
	AssetTracker.OnSelectionChanged();

	// Immediately check containment for newly selected actors
	if (AssetTracker.IsEnabled() && AssetTracker.GetTrackedActorCount() > 0)
	{
		TSet<APCGExValencyCage*> AffectedCages;
		TSet<APCGExValencyAssetPalette*> AffectedPalettes;
		if (AssetTracker.Update(AffectedCages, AffectedPalettes))
		{
			// Mark affected cages dirty - will be processed in next Tick
			for (APCGExValencyCage* Cage : AffectedCages)
			{
				if (Cage)
				{
					DirtyStateManager.MarkCageDirty(Cage, EValencyDirtyFlags::Assets);
				}
			}
			// Mark affected palettes dirty
			for (APCGExValencyAssetPalette* Palette : AffectedPalettes)
			{
				if (Palette)
				{
					DirtyStateManager.MarkPaletteDirty(Palette, EValencyDirtyFlags::Assets);
				}
			}
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
