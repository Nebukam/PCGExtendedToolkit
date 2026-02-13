// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "EditorMode/PCGExValencyCageEditorMode.h"

#include "EditorModeManager.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Selection.h"
#include "ScopedTransaction.h"
#include "LevelEditorViewport.h"
#include "UnrealEdGlobals.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "ToolContextInterfaces.h"

#include "EditorMode/PCGExValencyDrawHelper.h"
#include "EditorMode/PCGExValencyEditorModeToolkit.h"
#include "Cages/PCGExValencyCageBase.h"
#include "Cages/PCGExValencyCage.h"
#include "Cages/PCGExValencyCagePattern.h"
#include "Cages/PCGExValencyAssetPalette.h"
#include "Components/PCGExValencyCageSocketComponent.h"
#include "Volumes/ValencyContextVolume.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGExValencyCageEditorMode)

const FEditorModeID UPCGExValencyCageEditorMode::ModeID = TEXT("PCGExValencyCageEditorMode");

UPCGExValencyCageEditorMode::UPCGExValencyCageEditorMode()
{
	Info = FEditorModeInfo(
		ModeID,
		NSLOCTEXT("PCGExValency", "ValencyCageModeName", "PCGEx | Valency"),
		FSlateIcon(),
		true,
		MAX_int32);
}

FValencyReferenceTracker* UPCGExValencyCageEditorMode::GetActiveReferenceTracker()
{
	if (GLevelEditorModeTools().IsModeActive(ModeID))
	{
		if (UPCGExValencyCageEditorMode* Mode = Cast<UPCGExValencyCageEditorMode>(GLevelEditorModeTools().GetActiveScriptableMode(ModeID)))
		{
			return &Mode->ReferenceTracker;
		}
	}
	return nullptr;
}

void UPCGExValencyCageEditorMode::CreateToolkit()
{
	Toolkit = MakeShareable(new FPCGExValencyEditorModeToolkit());
}

void UPCGExValencyCageEditorMode::Enter()
{
	Super::Enter();

	// Bind to ITF rendering delegates (available after UEdMode::Enter creates the ToolsContext)
	if (UEditorInteractiveToolsContext* ToolsContext = GetInteractiveToolsContext())
	{
		OnRenderHandle = ToolsContext->OnRender.AddUObject(this, &UPCGExValencyCageEditorMode::OnRenderCallback);
		OnDrawHUDHandle = ToolsContext->OnDrawHUD.AddUObject(this, &UPCGExValencyCageEditorMode::OnDrawHUDCallback);
	}

	// Register keyboard command bindings on the toolkit
	if (Toolkit.IsValid())
	{
		TSharedPtr<FUICommandList> CommandList = Toolkit->GetToolkitCommands();
		if (CommandList.IsValid())
		{
			CommandList->MapAction(
				FValencyEditorCommands::Get().CleanupConnections,
				FExecuteAction::CreateUObject(this, &UPCGExValencyCageEditorMode::ExecuteCleanupCommand));

			CommandList->MapAction(
				FValencyEditorCommands::Get().AddSocket,
				FExecuteAction::CreateUObject(this, &UPCGExValencyCageEditorMode::ExecuteAddSocket),
				FCanExecuteAction::CreateUObject(this, &UPCGExValencyCageEditorMode::CanExecuteAddSocket));

			CommandList->MapAction(
				FValencyEditorCommands::Get().RemoveSocket,
				FExecuteAction::CreateUObject(this, &UPCGExValencyCageEditorMode::ExecuteRemoveSocket),
				FCanExecuteAction::CreateUObject(this, &UPCGExValencyCageEditorMode::CanExecuteRemoveSocket));

			CommandList->MapAction(
				FValencyEditorCommands::Get().DuplicateSocket,
				FExecuteAction::CreateUObject(this, &UPCGExValencyCageEditorMode::ExecuteDuplicateSocket),
				FCanExecuteAction::CreateUObject(this, &UPCGExValencyCageEditorMode::CanExecuteDuplicateSocket));

			CommandList->MapAction(
				FValencyEditorCommands::Get().ToggleSocketDirection,
				FExecuteAction::CreateUObject(this, &UPCGExValencyCageEditorMode::ExecuteToggleSocketDirection),
				FCanExecuteAction::CreateUObject(this, &UPCGExValencyCageEditorMode::CanExecuteToggleSocketDirection));
		}
	}

	// Bind to actor add/delete events to keep cache up to date
	if (GEditor)
	{
		OnActorAddedHandle = GEditor->OnLevelActorAdded().AddUObject(this, &UPCGExValencyCageEditorMode::OnLevelActorAdded);
		OnActorDeletedHandle = GEditor->OnLevelActorDeleted().AddUObject(this, &UPCGExValencyCageEditorMode::OnLevelActorDeleted);

		// Bind to selection changes for asset tracking
		OnSelectionChangedHandle = GEditor->GetSelectedActors()->SelectionChangedEvent.AddLambda([this](UObject*)
		{
			OnSelectionChanged();
		});

		// Bind to Undo/Redo for handling state restoration
		OnPostUndoRedoHandle = FEditorDelegates::PostUndoRedo.AddUObject(this, &UPCGExValencyCageEditorMode::OnPostUndoRedo);
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

void UPCGExValencyCageEditorMode::Exit()
{
	// Unbind ITF rendering delegates
	if (UEditorInteractiveToolsContext* ToolsContext = GetInteractiveToolsContext())
	{
		ToolsContext->OnRender.Remove(OnRenderHandle);
		ToolsContext->OnDrawHUD.Remove(OnDrawHUDHandle);
	}
	OnRenderHandle.Reset();
	OnDrawHUDHandle.Reset();

	// Unbind actor add/delete events
	if (GEditor)
	{
		GEditor->OnLevelActorAdded().Remove(OnActorAddedHandle);
		GEditor->OnLevelActorDeleted().Remove(OnActorDeletedHandle);
		GEditor->GetSelectedActors()->SelectionChangedEvent.Remove(OnSelectionChangedHandle);
	}
	FEditorDelegates::PostUndoRedo.Remove(OnPostUndoRedoHandle);

	OnActorAddedHandle.Reset();
	OnActorDeletedHandle.Reset();
	OnSelectionChangedHandle.Reset();
	OnPostUndoRedoHandle.Reset();

	// Clear tracking state
	AssetTracker.Reset();
	DirtyStateManager.Reset();
	ReferenceTracker.Reset();

	CachedCages.Empty();
	CachedVolumes.Empty();
	CachedPalettes.Empty();

	Super::Exit();
}

void UPCGExValencyCageEditorMode::OnRenderCallback(IToolsContextRenderAPI* RenderAPI)
{
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
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
	if (VisibilityFlags.bShowVolumes)
	{
		for (const TWeakObjectPtr<AValencyContextVolume>& VolumePtr : CachedVolumes)
		{
			if (AValencyContextVolume* Volume = VolumePtr.Get())
			{
				FPCGExValencyDrawHelper::DrawVolume(PDI, Volume);
			}
		}
	}

	// Draw asset palettes
	if (VisibilityFlags.bShowVolumes) // Palettes are volume-like containers
	{
		for (const TWeakObjectPtr<APCGExValencyAssetPalette>& PalettePtr : CachedPalettes)
		{
			if (APCGExValencyAssetPalette* Palette = PalettePtr.Get())
			{
				FPCGExValencyDrawHelper::DrawPalette(PDI, Palette);
			}
		}
	}

	// Draw cages (connections and patterns)
	if (VisibilityFlags.bShowConnections)
	{
		for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : CachedCages)
		{
			if (APCGExValencyCageBase* Cage = CagePtr.Get())
			{
				FPCGExValencyDrawHelper::DrawCage(PDI, Cage);
			}
		}
	}

	// Draw sockets for all cages
	if (VisibilityFlags.bShowSockets)
	{
		for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : CachedCages)
		{
			if (APCGExValencyCageBase* Cage = CagePtr.Get())
			{
				FPCGExValencyDrawHelper::DrawCageSockets(PDI, Cage);
			}
		}
	}
}

void UPCGExValencyCageEditorMode::OnDrawHUDCallback(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
	const FSceneView* View = RenderAPI->GetSceneView();
	if (!Canvas || !View)
	{
		return;
	}

	if (!VisibilityFlags.bShowLabels)
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

bool UPCGExValencyCageEditorMode::IsSelectionAllowed(AActor* InActor, bool bInSelection) const
{
	// Allow selection of all actors
	return true;
}

void UPCGExValencyCageEditorMode::ExecuteCleanupCommand()
{
	CleanupAllManualConnections();
}

void UPCGExValencyCageEditorMode::ModeTick(float DeltaTime)
{
	Super::ModeTick(DeltaTime);

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

void UPCGExValencyCageEditorMode::CollectCagesFromLevel()
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

void UPCGExValencyCageEditorMode::CollectVolumesFromLevel()
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

void UPCGExValencyCageEditorMode::CollectPalettesFromLevel()
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

void UPCGExValencyCageEditorMode::RefreshAllCages()
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
		if (APCGExValencyCageBase* CageBase = CagePtr.Get())
		{
			CageBase->RefreshGhostMeshes();
		}
	}
}

void UPCGExValencyCageEditorMode::InitializeCage(APCGExValencyCageBase* Cage)
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

	// Scan contained assets for regular cages
	if (APCGExValencyCage* RegularCage = Cast<APCGExValencyCage>(Cage))
	{
		if (RegularCage->bAutoRegisterContainedAssets)
		{
			RegularCage->ScanAndRegisterContainedAssets();
		}
	}

	// Refresh ghost meshes (virtual - dispatches to correct subclass)
	Cage->RefreshGhostMeshes();
}

void UPCGExValencyCageEditorMode::OnLevelActorAdded(AActor* Actor)
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

		// Notify scene changed
		OnSceneChanged.Broadcast();

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
		OnSceneChanged.Broadcast();
	}
	// Check if it's an asset palette
	else if (APCGExValencyAssetPalette* Palette = Cast<APCGExValencyAssetPalette>(Actor))
	{
		CachedPalettes.Add(Palette);
		// Palettes can be MirrorSources - rebuild dependency graph
		ReferenceTracker.RebuildDependencyGraph();
		OnSceneChanged.Broadcast();
	}
}

void UPCGExValencyCageEditorMode::OnLevelActorDeleted(AActor* Actor)
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

		// Capture dependents BEFORE rebuilding the dependency graph
		// Cages that mirrored/proxied the deleted cage need their ghost meshes refreshed
		TArray<AActor*> Dependents;
		ReferenceTracker.CollectAffectedActors(Actor, Dependents);

		// Remove from cache
		CachedCages.RemoveAll([Cage](const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr)
		{
			return CagePtr.Get() == Cage || !CagePtr.IsValid();
		});

		// Rebuild dependency graph - removed cage may have been a dependency
		ReferenceTracker.RebuildDependencyGraph();

		// Refresh ghost meshes on cages that depended on the deleted actor
		for (AActor* Dependent : Dependents)
		{
			ReferenceTracker.RefreshDependentVisuals(Dependent);
		}

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

		OnSceneChanged.Broadcast();
	}
	// Check if it's a volume
	else if (AValencyContextVolume* Volume = Cast<AValencyContextVolume>(Actor))
	{
		CachedVolumes.RemoveAll([Volume](const TWeakObjectPtr<AValencyContextVolume>& VolumePtr)
		{
			return VolumePtr.Get() == Volume || !VolumePtr.IsValid();
		});
		bCacheDirty = true; // Volumes affect cage orbital sets - full refresh needed
		OnSceneChanged.Broadcast();
	}
	// Check if it's an asset palette
	else if (APCGExValencyAssetPalette* Palette = Cast<APCGExValencyAssetPalette>(Actor))
	{
		// Capture dependents BEFORE rebuilding the dependency graph
		TArray<AActor*> Dependents;
		ReferenceTracker.CollectAffectedActors(Actor, Dependents);

		CachedPalettes.RemoveAll([Palette](const TWeakObjectPtr<APCGExValencyAssetPalette>& PalettePtr)
		{
			return PalettePtr.Get() == Palette || !PalettePtr.IsValid();
		});

		// Rebuild dependency graph - removed palette may have been a dependency
		ReferenceTracker.RebuildDependencyGraph();

		// Refresh ghost meshes on cages that depended on the deleted palette
		for (AActor* Dependent : Dependents)
		{
			ReferenceTracker.RefreshDependentVisuals(Dependent);
		}

		OnSceneChanged.Broadcast();
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

void UPCGExValencyCageEditorMode::OnSelectionChanged()
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

void UPCGExValencyCageEditorMode::OnPostUndoRedo()
{
	// After Undo/Redo, the level state may have changed in ways that bypass OnLevelActorAdded/Deleted.
	// We need to:
	// 1. Re-collect cages (some may have been added/removed by undo)
	// 2. Refresh all cage connections
	// 3. Mark all volumes dirty to trigger rebuild

	// Re-collect from level to catch any added/removed actors
	CollectCagesFromLevel();
	CollectVolumesFromLevel();
	CollectPalettesFromLevel();

	// Rebuild dependency graph
	ReferenceTracker.RebuildDependencyGraph();

	// Refresh ALL cage connections - undo/redo can affect any cage
	for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : CachedCages)
	{
		if (APCGExValencyCageBase* Cage = CagePtr.Get())
		{
			if (!Cage->IsNullCage())
			{
				Cage->RefreshContainingVolumes();
				Cage->DetectNearbyConnections();
			}
		}
	}

	// Mark ALL volumes dirty to trigger rebuild with fresh data
	for (const TWeakObjectPtr<AValencyContextVolume>& VolumePtr : CachedVolumes)
	{
		if (AValencyContextVolume* Volume = VolumePtr.Get())
		{
			DirtyStateManager.MarkVolumeDirty(Volume, EValencyDirtyFlags::Structure);
		}
	}

	OnSceneChanged.Broadcast();
	RedrawViewports();
}

void UPCGExValencyCageEditorMode::SetAllCageDebugComponentsVisible(bool bVisible)
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

int32 UPCGExValencyCageEditorMode::CleanupAllManualConnections()
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

void UPCGExValencyCageEditorMode::RedrawViewports()
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

// ========== Widget Interface ==========

bool UPCGExValencyCageEditorMode::UsesTransformWidget() const
{
	return true;
}

bool UPCGExValencyCageEditorMode::UsesTransformWidget(UE::Widget::EWidgetMode CheckMode) const
{
	return true;
}

bool UPCGExValencyCageEditorMode::ShouldDrawWidget() const
{
	return true;
}

// ========== Socket Management ==========

UPCGExValencyCageSocketComponent* UPCGExValencyCageEditorMode::GetSelectedSocket()
{
	if (!GEditor)
	{
		return nullptr;
	}

	if (USelection* CompSelection = GEditor->GetSelectedComponents())
	{
		for (FSelectionIterator It(*CompSelection); It; ++It)
		{
			if (UPCGExValencyCageSocketComponent* Socket = Cast<UPCGExValencyCageSocketComponent>(*It))
			{
				return Socket;
			}
		}
	}

	return nullptr;
}

APCGExValencyCageBase* UPCGExValencyCageEditorMode::GetSelectedCage()
{
	if (!GEditor)
	{
		return nullptr;
	}

	USelection* Selection = GEditor->GetSelectedActors();
	for (FSelectionIterator It(*Selection); It; ++It)
	{
		if (APCGExValencyCageBase* Cage = Cast<APCGExValencyCageBase>(*It))
		{
			return Cage;
		}
	}

	return nullptr;
}

UPCGExValencyCageSocketComponent* UPCGExValencyCageEditorMode::AddSocketToCage(APCGExValencyCageBase* Cage)
{
	if (!Cage || !GEditor)
	{
		return nullptr;
	}

	FScopedTransaction Transaction(NSLOCTEXT("PCGExValency", "AddSocket", "Add Socket"));
	Cage->Modify();

	UPCGExValencyCageSocketComponent* NewSocket = NewObject<UPCGExValencyCageSocketComponent>(Cage, NAME_None, RF_Transactional);
	NewSocket->CreationMethod = EComponentCreationMethod::Instance;
	NewSocket->AttachToComponent(Cage->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	NewSocket->RegisterComponent();
	Cage->AddInstanceComponent(NewSocket);

	// Select the new socket
	GEditor->SelectActor(Cage, true, true);
	GEditor->SelectComponent(NewSocket, true, true);

	// Trigger rebuild and refresh
	Cage->RequestRebuild(EValencyRebuildReason::AssetChange);
	OnSceneChanged.Broadcast();
	RedrawViewports();

	return NewSocket;
}

void UPCGExValencyCageEditorMode::RemoveSocket(UPCGExValencyCageSocketComponent* Socket)
{
	if (!Socket || !GEditor)
	{
		return;
	}

	APCGExValencyCageBase* Cage = Cast<APCGExValencyCageBase>(Socket->GetOwner());
	if (!Cage)
	{
		return;
	}

	FScopedTransaction Transaction(NSLOCTEXT("PCGExValency", "RemoveSocket", "Remove Socket"));
	Cage->Modify();
	Socket->Modify();

	// Clear selection before destroy
	GEditor->SelectComponent(Socket, false, true);

	Cage->RemoveInstanceComponent(Socket);
	Socket->DestroyComponent();

	// Trigger rebuild and refresh
	Cage->RequestRebuild(EValencyRebuildReason::AssetChange);
	OnSceneChanged.Broadcast();
	RedrawViewports();
}

UPCGExValencyCageSocketComponent* UPCGExValencyCageEditorMode::DuplicateSocket(UPCGExValencyCageSocketComponent* Socket)
{
	if (!Socket || !GEditor)
	{
		return nullptr;
	}

	APCGExValencyCageBase* Cage = Cast<APCGExValencyCageBase>(Socket->GetOwner());
	if (!Cage)
	{
		return nullptr;
	}

	FScopedTransaction Transaction(NSLOCTEXT("PCGExValency", "DuplicateSocket", "Duplicate Socket"));
	Cage->Modify();

	UPCGExValencyCageSocketComponent* NewSocket = NewObject<UPCGExValencyCageSocketComponent>(Cage, NAME_None, RF_Transactional);
	NewSocket->CreationMethod = EComponentCreationMethod::Instance;
	NewSocket->AttachToComponent(Cage->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	NewSocket->RegisterComponent();
	Cage->AddInstanceComponent(NewSocket);

	// Copy properties from source
	NewSocket->SocketType = Socket->SocketType;
	NewSocket->bIsOutputSocket = Socket->bIsOutputSocket;
	NewSocket->bEnabled = Socket->bEnabled;
	NewSocket->DebugColorOverride = Socket->DebugColorOverride;

	// Offset the duplicate slightly in local X
	FTransform SourceTransform = Socket->GetRelativeTransform();
	SourceTransform.AddToTranslation(FVector(20.0, 0.0, 0.0));
	NewSocket->SetRelativeTransform(SourceTransform);

	// Select the new socket
	GEditor->SelectActor(Cage, true, true);
	GEditor->SelectComponent(NewSocket, true, true);

	// Trigger rebuild and refresh
	Cage->RequestRebuild(EValencyRebuildReason::AssetChange);
	OnSceneChanged.Broadcast();
	RedrawViewports();

	return NewSocket;
}

// ========== Socket Command Execute/CanExecute ==========

void UPCGExValencyCageEditorMode::ExecuteAddSocket()
{
	if (APCGExValencyCageBase* Cage = GetSelectedCage())
	{
		AddSocketToCage(Cage);
	}
}

bool UPCGExValencyCageEditorMode::CanExecuteAddSocket() const
{
	return GetSelectedCage() != nullptr;
}

void UPCGExValencyCageEditorMode::ExecuteRemoveSocket()
{
	if (UPCGExValencyCageSocketComponent* Socket = GetSelectedSocket())
	{
		RemoveSocket(Socket);
	}
}

bool UPCGExValencyCageEditorMode::CanExecuteRemoveSocket() const
{
	return GetSelectedSocket() != nullptr;
}

void UPCGExValencyCageEditorMode::ExecuteDuplicateSocket()
{
	if (UPCGExValencyCageSocketComponent* Socket = GetSelectedSocket())
	{
		DuplicateSocket(Socket);
	}
}

bool UPCGExValencyCageEditorMode::CanExecuteDuplicateSocket() const
{
	return GetSelectedSocket() != nullptr;
}

void UPCGExValencyCageEditorMode::ExecuteToggleSocketDirection()
{
	UPCGExValencyCageSocketComponent* Socket = GetSelectedSocket();
	if (!Socket)
	{
		return;
	}

	FScopedTransaction Transaction(NSLOCTEXT("PCGExValency", "ToggleSocketDir", "Toggle Socket Direction"));
	Socket->Modify();
	Socket->bIsOutputSocket = !Socket->bIsOutputSocket;

	if (APCGExValencyCageBase* Cage = Cast<APCGExValencyCageBase>(Socket->GetOwner()))
	{
		Cage->RequestRebuild(EValencyRebuildReason::AssetChange);
	}

	OnSceneChanged.Broadcast();
	RedrawViewports();
}

bool UPCGExValencyCageEditorMode::CanExecuteToggleSocketDirection() const
{
	return GetSelectedSocket() != nullptr;
}
