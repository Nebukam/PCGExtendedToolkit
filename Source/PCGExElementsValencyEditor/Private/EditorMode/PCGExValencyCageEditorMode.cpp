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

			// Draw cage name label
			const FVector CageLocation = Cage->GetActorLocation();
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
	// TODO: Phase 3 - implement hotkeys
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

	// Use probe radius for arrow length
	const float ArrowLength = Cage->GetEffectiveProbeRadius();

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

		// Determine color and style based on connection state
		FLinearColor ArrowColor;
		bool bDashed = false;
		bool bDrawArrowhead = true;

		if (!Orbital.bEnabled)
		{
			// Disabled orbital - skip drawing
			continue;
		}
		else if (Orbital.ConnectedCage.IsValid())
		{
			const APCGExValencyCageBase* ConnectedCage = Orbital.ConnectedCage.Get();

			if (ConnectedCage->IsNullCage())
			{
				// Connection to null cage: dashed darkish red, no arrowhead
				ArrowColor = NullConnectionColor;
				bDashed = true;
				bDrawArrowhead = false;
			}
			else if (ConnectedCage->HasConnectionTo(Cage))
			{
				// Bidirectional connection: green with arrowhead
				ArrowColor = BidirectionalColor;
			}
			else
			{
				// Unilateral connection: teal with arrowhead
				ArrowColor = UnilateralColor;
			}

			// Draw connection line to connected cage
			DrawConnection(PDI, Cage, Orbital.OrbitalIndex, ConnectedCage);
		}
		else
		{
			// No connection: dashed light gray, no arrowhead
			ArrowColor = NoConnectionColor;
			bDashed = true;
			bDrawArrowhead = false;
		}

		// Draw the orbital arrow
		DrawOrbitalArrow(PDI, CageLocation, WorldDir, ArrowLength, ArrowColor, bDashed, bDrawArrowhead);
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

void FPCGExValencyCageEditorMode::DrawConnection(FPrimitiveDrawInterface* PDI, const APCGExValencyCageBase* FromCage, int32 OrbitalIndex, const APCGExValencyCageBase* ToCage)
{
	if (!FromCage || !ToCage)
	{
		return;
	}

	const FVector From = FromCage->GetActorLocation();
	const FVector To = ToCage->GetActorLocation();

	// Determine connection type and color
	const bool bToNullCage = ToCage->IsNullCage();
	const bool bMutual = !bToNullCage && ToCage->HasConnectionTo(FromCage);

	FLinearColor LineColor;
	if (bToNullCage)
	{
		LineColor = NullConnectionColor;
	}
	else if (bMutual)
	{
		LineColor = BidirectionalColor;
	}
	else
	{
		LineColor = UnilateralColor;
	}

	PDI->DrawLine(From, To, LineColor, SDPG_World, ConnectionLineThickness);

	// Draw arrowhead at midpoint pointing toward target (only for unilateral, not for null or bidirectional)
	if (!bMutual && !bToNullCage)
	{
		const FVector Mid = (From + To) * 0.5f;
		const FVector Dir = (To - From).GetSafeNormal();
		const FVector Right = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();

		const float ArrowSize = 15.0f;
		PDI->DrawLine(Mid, Mid - Dir * ArrowSize + Right * ArrowSize * 0.5f, LineColor, SDPG_World, ConnectionLineThickness);
		PDI->DrawLine(Mid, Mid - Dir * ArrowSize - Right * ArrowSize * 0.5f, LineColor, SDPG_World, ConnectionLineThickness);
	}
}

void FPCGExValencyCageEditorMode::DrawOrbitalArrow(FPrimitiveDrawInterface* PDI, const FVector& Origin, const FVector& Direction, float Length, const FLinearColor& Color, bool bDashed, bool bDrawArrowhead)
{
	// Arrow geometry when arrowhead is drawn:
	// - 0% to 70%: thin line
	// - 70% to 110%: thicker arrow line with arrowhead at 110%
	// Without arrowhead: just draw to 100%
	const float ThinEndPct = bDrawArrowhead ? 0.70f : 1.0f;
	const float ArrowEndPct = 1.10f;

	const FVector ThinEnd = Origin + Direction * (Length * ThinEndPct);
	const FVector ArrowEnd = Origin + Direction * (Length * ArrowEndPct);

	if (bDashed)
	{
		// Draw dashed line (0% to ThinEndPct)
		const float ThinLength = Length * ThinEndPct;
		const int32 NumSegments = FMath::Max(4, FMath::RoundToInt(ThinLength / 25.0f)); // Scale segments with length
		const float SegmentLength = ThinLength / (NumSegments * 2);

		for (int32 i = 0; i < NumSegments; ++i)
		{
			const float Start = i * SegmentLength * 2;
			const float End = Start + SegmentLength;

			const FVector SegStart = Origin + Direction * Start;
			const FVector SegEnd = Origin + Direction * FMath::Min(End, ThinLength);

			PDI->DrawLine(SegStart, SegEnd, Color, SDPG_World, 1.0f);
		}

		if (bDrawArrowhead)
		{
			// Draw dashed arrow line (70% to 110%)
			const float ArrowLength = Length * (ArrowEndPct - ThinEndPct);
			const int32 NumArrowSegments = 2;
			const float ArrowSegmentLength = ArrowLength / (NumArrowSegments * 2);

			for (int32 i = 0; i < NumArrowSegments; ++i)
			{
				const float Start = i * ArrowSegmentLength * 2;
				const float End = Start + ArrowSegmentLength;

				const FVector SegStart = ThinEnd + Direction * Start;
				const FVector SegEnd = ThinEnd + Direction * FMath::Min(End, ArrowLength);

				PDI->DrawLine(SegStart, SegEnd, Color, SDPG_World, 2.0f);
			}
		}
	}
	else
	{
		// Solid thin line (0% to ThinEndPct)
		PDI->DrawLine(Origin, ThinEnd, Color, SDPG_World, 1.0f);

		if (bDrawArrowhead)
		{
			// Solid thicker arrow line (70% to 110%)
			PDI->DrawLine(ThinEnd, ArrowEnd, Color, SDPG_World, 2.0f);
		}
	}

	// Draw arrowhead at 110% if enabled
	if (bDrawArrowhead)
	{
		const FVector Right = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
		const FVector Up = FVector::CrossProduct(Right, Direction).GetSafeNormal();
		const float ArrowSize = 10.0f;

		PDI->DrawLine(ArrowEnd, ArrowEnd - Direction * ArrowSize + Right * ArrowSize * 0.5f, Color, SDPG_World, 2.0f);
		PDI->DrawLine(ArrowEnd, ArrowEnd - Direction * ArrowSize - Right * ArrowSize * 0.5f, Color, SDPG_World, 2.0f);
		PDI->DrawLine(ArrowEnd, ArrowEnd - Direction * ArrowSize + Up * ArrowSize * 0.5f, Color, SDPG_World, 2.0f);
		PDI->DrawLine(ArrowEnd, ArrowEnd - Direction * ArrowSize - Up * ArrowSize * 0.5f, Color, SDPG_World, 2.0f);
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
