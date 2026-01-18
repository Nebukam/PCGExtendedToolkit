// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "EditorMode/PCGExValencyCageEditorMode.h"

#include "EditorModeManager.h"
#include "EngineUtils.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "SceneManagement.h"
#include "Editor.h"
#include "Selection.h"

#include "Cages/PCGExValencyCageBase.h"
#include "Volumes/ValencyContextVolume.h"
#include "Core/PCGExValencyOrbitalSet.h"

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
	}

	// Collect and fully initialize all cages
	CollectCagesFromLevel();
	CollectVolumesFromLevel();
	RefreshAllCages();

	bCacheDirty = false;
}

void FPCGExValencyCageEditorMode::Exit()
{
	// Unbind actor add/delete events
	if (GEditor)
	{
		GEditor->OnLevelActorAdded().Remove(OnActorAddedHandle);
		GEditor->OnLevelActorDeleted().Remove(OnActorDeletedHandle);
	}
	OnActorAddedHandle.Reset();
	OnActorDeletedHandle.Reset();

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
			DrawVolume(PDI, Volume);
		}
	}

	// Draw cages
	for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : CachedCages)
	{
		if (APCGExValencyCageBase* Cage = CagePtr.Get())
		{
			DrawCage(PDI, Cage);
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

	// Determine which cages are selected for label opacity
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

	// Label colors - full opacity for selected, faint for unselected
	const FLinearColor SelectedLabelColor = FLinearColor::White;
	const FLinearColor UnselectedLabelColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.35f);

	// Draw orbital labels on cages
	for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : CachedCages)
	{
		if (const APCGExValencyCageBase* Cage = CagePtr.Get())
		{
			const bool bIsSelected = SelectedCages.Contains(Cage);
			const FLinearColor LabelColor = bIsSelected ? SelectedLabelColor : UnselectedLabelColor;
			const FVector CageLocation = Cage->GetActorLocation();

			// Null cages: just show "NULL Cage" label, no orbitals
			if (Cage->IsNullCage())
			{
				DrawLabel(Canvas, View, CageLocation + FVector(0, 0, 50), TEXT("NULL Cage"), LabelColor);
				continue;
			}

			// Draw cage name label
			const FString CageName = Cage->GetCageDisplayName();

			if (!CageName.IsEmpty())
			{
				DrawLabel(Canvas, View, CageLocation + FVector(0, 0, 50), CageName, LabelColor);
			}

			// Draw orbital labels if cage has an orbital set
			const UPCGExValencyOrbitalSet* OrbitalSet = Cage->GetEffectiveOrbitalSet();
			if (OrbitalSet)
			{
				const TArray<FPCGExValencyCageOrbital>& Orbitals = Cage->GetOrbitals();
				const FTransform CageTransform = Cage->GetActorTransform();
				const float ProbeRadius = Cage->GetEffectiveProbeRadius();

				for (const FPCGExValencyCageOrbital& Orbital : Orbitals)
				{
					if (Orbital.OrbitalIndex >= 0 && Orbital.OrbitalIndex < OrbitalSet->Num())
					{
						const FPCGExValencyOrbitalEntry& Entry = OrbitalSet->Orbitals[Orbital.OrbitalIndex];

						// Get direction from bitmask reference
						FVector Direction;
						int64 Bitmask;
						if (Entry.GetDirectionAndBitmask(Direction, Bitmask))
						{
							FVector WorldDir = CageTransform.TransformVectorNoScale(Direction);
							WorldDir.Normalize();

							// Position at 30% of probe radius to avoid overlap with incoming labels
							const FVector LabelPos = CageLocation + WorldDir * (ProbeRadius * 0.3f);
							DrawLabel(Canvas, View, LabelPos, Entry.GetDisplayName().ToString(), LabelColor);
						}
					}
				}
			}
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
	// Allow selection of cages and volumes
	if (Cast<APCGExValencyCageBase>(InActor) || Cast<AValencyContextVolume>(InActor))
	{
		return true;
	}

	// Allow other actors too
	return true;
}

void FPCGExValencyCageEditorMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	FEdMode::Tick(ViewportClient, DeltaTime);

	// Check for actor changes that would invalidate cache
	// TODO: Hook into actor add/remove/move delegates
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

void FPCGExValencyCageEditorMode::DrawCage(FPrimitiveDrawInterface* PDI, const APCGExValencyCageBase* Cage)
{
	if (!Cage)
	{
		return;
	}

	const FVector CageLocation = Cage->GetActorLocation();
	const FTransform CageTransform = Cage->GetActorTransform();
	const bool bIsNullCage = Cage->IsNullCage();

	// Get the orbital set for direction info
	const UPCGExValencyOrbitalSet* OrbitalSet = Cage->GetEffectiveOrbitalSet();

	// Null cages are valid without orbital sets - they're boundary markers
	// They have their own debug sphere component, no additional drawing needed
	if (bIsNullCage)
	{
		return;
	}

	// Non-null cages need an orbital set
	if (!OrbitalSet)
	{
		// Draw warning box if no orbital set
		DrawWireBox(PDI, FBox(CageLocation - FVector(25), CageLocation + FVector(25)), WarningColor, SDPG_World);
		return;
	}

	// Use probe radius for calculating arrow positions
	const float ProbeRadius = Cage->GetEffectiveProbeRadius();
	const float StartOffset = ProbeRadius * ArrowStartOffsetPct;

	// Note: Simple cages have their own debug shape components for bounds visualization
	// We only draw orbital arrows and connections here

	// Draw orbital arrows
	const TArray<FPCGExValencyCageOrbital>& Orbitals = Cage->GetOrbitals();

	for (const FPCGExValencyCageOrbital& Orbital : Orbitals)
	{
		if (Orbital.OrbitalIndex < 0 || Orbital.OrbitalIndex >= OrbitalSet->Num())
		{
			continue;
		}

		const FPCGExValencyOrbitalEntry& Entry = OrbitalSet->Orbitals[Orbital.OrbitalIndex];

		// Get direction from bitmask reference
		FVector Direction;
		int64 Bitmask;
		if (!Entry.GetDirectionAndBitmask(Direction, Bitmask))
		{
			continue;
		}

		// Transform orbital direction to world space
		FVector WorldDir = CageTransform.TransformVectorNoScale(Direction);
		WorldDir.Normalize();

		if (!Orbital.bEnabled)
		{
			// Disabled orbital - skip drawing
			continue;
		}
		else if (const APCGExValencyCageBase* ConnectedCage = Orbital.GetDisplayConnection())
		{
			const FVector ConnectedLocation = ConnectedCage->GetActorLocation();
			const float ConnectionDistance = FVector::Dist(CageLocation, ConnectedLocation);
			const FVector ToConnected = (ConnectedLocation - CageLocation).GetSafeNormal();

			// Thick arrow starts offset from center in direction toward connected cage
			const FVector ArrowStart = CageLocation + ToConnected * StartOffset;

			// Thin line length: shorter when connected to reduce noise
			const float ThinLineLength = ProbeRadius * 0.5f;

			FLinearColor ArrowColor;
			bool bDashed = false;
			bool bDrawArrowhead = true;
			FVector ArrowEnd;

			if (ConnectedCage->IsNullCage())
			{
				// Connection to null cage: dashed darkish red, no arrowhead on thick arrow
				ArrowColor = NullConnectionColor;
				bDashed = true;
				bDrawArrowhead = false;

				// Thick arrow ends at null cage's sphere surface
				const float NullSphereRadius = ConnectedCage->GetEffectiveProbeRadius();
				const float DistanceToSurface = ConnectionDistance - NullSphereRadius;
				ArrowEnd = CageLocation + ToConnected * FMath::Max(DistanceToSurface, StartOffset + 10.0f);
			}
			else
			{
				// Thick arrow ends at midpoint for regular connections
				ArrowEnd = CageLocation + ToConnected * (ConnectionDistance * 0.5f);

				if (ConnectedCage->HasConnectionTo(Cage))
				{
					// Bidirectional connection: green with arrowhead
					ArrowColor = BidirectionalColor;
				}
				else
				{
					// Unilateral connection: teal with arrowhead
					ArrowColor = UnilateralColor;
				}
			}

			// Draw thin line along orbital direction (WorldDir), shorter length when connected
			DrawConnection(PDI, CageLocation, WorldDir, ThinLineLength, ArrowColor, false, false);

			// Draw the thick arrow - points toward connected cage
			DrawOrbitalArrow(PDI, ArrowStart, ArrowEnd, ArrowColor, bDashed, bDrawArrowhead, 1.5f);
		}
		else
		{
			// No connection: draw dashed thin line along orbital direction at full probe radius
			// with arrowhead to show direction
			DrawConnection(PDI, CageLocation, WorldDir, ProbeRadius, NoConnectionColor, true, true);
		}
	}
}

void FPCGExValencyCageEditorMode::DrawVolume(FPrimitiveDrawInterface* PDI, const AValencyContextVolume* Volume)
{
	if (!Volume)
	{
		return;
	}

	// Volumes handle their own brush rendering, but we can add overlay
	// Get volume bounds and draw with our color
	FVector Origin, BoxExtent;
	Volume->GetActorBounds(false, Origin, BoxExtent);

	const FBox VolumeBox(Origin - BoxExtent, Origin + BoxExtent);

	// Use volume's debug color if available, otherwise default
	const FLinearColor DrawColor = Volume->DebugColor;

	DrawWireBox(PDI, VolumeBox, DrawColor, SDPG_World);
}

void FPCGExValencyCageEditorMode::DrawConnection(FPrimitiveDrawInterface* PDI, const FVector& From, const FVector& Along, float Distance, const FLinearColor& Color, bool bDrawArrowhead, bool bDashed)
{
	const FVector To = From + Along * Distance;

	if (bDashed)
	{
		// Draw dashed line
		const float DashCycle = DashLength + DashGap;
		float CurrentPos = 0.0f;

		while (CurrentPos < Distance)
		{
			const float DashStart = CurrentPos;
			const float DashEnd = FMath::Min(CurrentPos + DashLength, Distance);

			const FVector SegStart = From + Along * DashStart;
			const FVector SegEnd = From + Along * DashEnd;

			PDI->DrawLine(SegStart, SegEnd, Color, SDPG_World, ConnectionLineThickness);
			CurrentPos += DashCycle;
		}
	}
	else
	{
		PDI->DrawLine(From, To, Color, SDPG_World, ConnectionLineThickness);
	}

	// Draw arrowhead at end if requested
	if (bDrawArrowhead)
	{
		const FVector Right = FVector::CrossProduct(Along, FVector::UpVector).GetSafeNormal();

		const float ArrowSize = 12.0f;
		PDI->DrawLine(To, To - Along * ArrowSize + Right * ArrowSize * 0.5f, Color, SDPG_World, ConnectionLineThickness);
		PDI->DrawLine(To, To - Along * ArrowSize - Right * ArrowSize * 0.5f, Color, SDPG_World, ConnectionLineThickness);
	}
}

void FPCGExValencyCageEditorMode::DrawOrbitalArrow(FPrimitiveDrawInterface* PDI, const FVector& Start, const FVector& End, const FLinearColor& Color, bool bDashed, bool bDrawArrowhead, float Thickness, float ThicknessArrow)
{
	const FVector Direction = (End - Start).GetSafeNormal();
	const float TotalLength = FVector::Dist(Start, End);

	if (TotalLength < KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Arrow geometry when arrowhead is drawn:
	// - Main line up to 85% of length
	// - Arrowhead prongs at the end
	const float MainLinePct = bDrawArrowhead ? 0.85f : 1.0f;
	const FVector MainLineEnd = Start + Direction * (TotalLength * MainLinePct);

	if (bDashed)
	{
		// Draw dashed line using configurable dash length/gap
		const float MainLength = TotalLength * MainLinePct;
		const float DashCycle = DashLength + DashGap;
		float CurrentPos = 0.0f;

		while (CurrentPos < MainLength)
		{
			const float DashStart = CurrentPos;
			const float DashEnd = FMath::Min(CurrentPos + DashLength, MainLength);

			const FVector SegStart = Start + Direction * DashStart;
			const FVector SegEnd = Start + Direction * DashEnd;

			PDI->DrawLine(SegStart, SegEnd, Color, SDPG_World, Thickness);
			CurrentPos += DashCycle;
		}
	}
	else
	{
		// Solid line to main end point
		PDI->DrawLine(Start, MainLineEnd, Color, SDPG_World, Thickness);
	}

	// Draw arrowhead at end if enabled
	if (bDrawArrowhead)
	{
		const FVector Right = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
		const FVector Up = FVector::CrossProduct(Right, Direction).GetSafeNormal();
		const float ArrowSize = 10.0f;

		// Four prongs for 3D arrowhead
		PDI->DrawLine(End, End - Direction * ArrowSize + Right * ArrowSize * 0.5f, Color, SDPG_World, ThicknessArrow);
		PDI->DrawLine(End, End - Direction * ArrowSize - Right * ArrowSize * 0.5f, Color, SDPG_World, ThicknessArrow);
		PDI->DrawLine(End, End - Direction * ArrowSize + Up * ArrowSize * 0.5f, Color, SDPG_World, ThicknessArrow);
		PDI->DrawLine(End, End - Direction * ArrowSize - Up * ArrowSize * 0.5f, Color, SDPG_World, ThicknessArrow);
	}
}

void FPCGExValencyCageEditorMode::DrawLabel(FCanvas* Canvas, const FSceneView* View, const FVector& WorldLocation, const FString& Text, const FLinearColor& Color)
{
	if (!Canvas || !View || Text.IsEmpty())
	{
		return;
	}

	// Project world location to screen
	FVector2D ScreenPos;
	if (View->WorldToPixel(WorldLocation, ScreenPos))
	{
		// Check if on screen
		const FIntRect ViewRect = View->UnscaledViewRect;
		if (ScreenPos.X >= ViewRect.Min.X && ScreenPos.X <= ViewRect.Max.X &&
			ScreenPos.Y >= ViewRect.Min.Y && ScreenPos.Y <= ViewRect.Max.Y)
		{
			// Draw text
			FCanvasTextItem TextItem(ScreenPos, FText::FromString(Text), GEngine->GetSmallFont(), Color);
			TextItem.bCentreX = true;
			TextItem.bCentreY = true;
			TextItem.EnableShadow(FLinearColor::Black);
			Canvas->DrawItem(TextItem);
		}
	}
}

void FPCGExValencyCageEditorMode::SetAllCageDebugComponentsVisible(bool bVisible)
{
	// Use cached cages if available, otherwise collect from level
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

		// Refresh connections on remaining cages (their connection to deleted cage is now invalid)
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
			// This handles: new cages, loaded cages, duplicated cages, volume changes
			// InitializeOrbitalsFromSet preserves existing connections where indices match
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

	// Redraw to reflect any changes
	if (TotalRemoved > 0 && GEditor)
	{
		GEditor->RedrawAllViewports();
	}

	return TotalRemoved;
}
