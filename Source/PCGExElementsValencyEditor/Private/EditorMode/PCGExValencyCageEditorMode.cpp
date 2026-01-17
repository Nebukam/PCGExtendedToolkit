// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "EditorMode/PCGExValencyCageEditorMode.h"

#include "EditorModeManager.h"
#include "EngineUtils.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "SceneManagement.h"

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

	// Refresh caches on enter
	bCacheDirty = true;
	CollectCagesFromLevel();
	CollectVolumesFromLevel();
}

void FPCGExValencyCageEditorMode::Exit()
{
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

	// Draw orbital labels on cages
	for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : CachedCages)
	{
		if (const APCGExValencyCageBase* Cage = CagePtr.Get())
		{
			// Draw cage name label
			const FVector CageLocation = Cage->GetActorLocation();
			const FString CageName = Cage->GetCageDisplayName();

			if (!CageName.IsEmpty())
			{
				DrawLabel(Canvas, View, CageLocation + FVector(0, 0, 50), CageName, FLinearColor::White);
			}

			// Draw orbital labels if cage has an orbital set
			const UPCGExValencyOrbitalSet* OrbitalSet = Cage->GetEffectiveOrbitalSet();
			if (OrbitalSet)
			{
				const TArray<FPCGExValencyCageOrbital>& Orbitals = Cage->GetOrbitals();
				const FTransform CageTransform = Cage->GetActorTransform();

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

							const FVector LabelPos = CageLocation + WorldDir * (OrbitalArrowLength * 0.5f);
							DrawLabel(Canvas, View, LabelPos, Entry.GetOrbitalName().ToString(), FLinearColor::White);
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

	// Get the orbital set for direction info
	const UPCGExValencyOrbitalSet* OrbitalSet = Cage->GetEffectiveOrbitalSet();
	if (!OrbitalSet)
	{
		// Draw warning box if no orbital set
		DrawWireBox(PDI, FBox(CageLocation - FVector(25), CageLocation + FVector(25)), WarningColor, SDPG_World);
		return;
	}

	// Draw cage center marker
	const bool bIsNullCage = Cage->IsNullCage();
	const FLinearColor CenterColor = bIsNullCage ? NullCageColor : FLinearColor::White;

	// Draw small sphere at cage center
	DrawWireSphere(PDI, CageLocation, CenterColor, 10.0f, 8, SDPG_World);

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

		// Determine color based on connection state
		FLinearColor ArrowColor;
		bool bDashed = false;

		if (!Orbital.bEnabled)
		{
			ArrowColor = DisconnectedColor * 0.5f;
			bDashed = true;
		}
		else if (Orbital.ConnectedCage.IsValid())
		{
			// Check if connection is mutual
			const APCGExValencyCageBase* ConnectedCage = Orbital.ConnectedCage.Get();
			if (ConnectedCage && ConnectedCage->HasConnectionTo(Cage))
			{
				ArrowColor = MutualConnectionColor;
			}
			else
			{
				ArrowColor = AsymmetricConnectionColor;
				bDashed = true;
			}

			// Draw connection line to connected cage
			if (ConnectedCage)
			{
				DrawConnection(PDI, Cage, Orbital.OrbitalIndex, ConnectedCage);
			}
		}
		else
		{
			ArrowColor = DisconnectedColor;
		}

		// Draw the orbital arrow
		DrawOrbitalArrow(PDI, CageLocation, WorldDir, OrbitalArrowLength, ArrowColor, bDashed);
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

	// Check if mutual
	const bool bMutual = ToCage->HasConnectionTo(FromCage);
	const FLinearColor LineColor = bMutual ? MutualConnectionColor : AsymmetricConnectionColor;

	PDI->DrawLine(From, To, LineColor, SDPG_World, ConnectionLineThickness);

	// Draw arrowhead at midpoint pointing toward target
	if (!bMutual)
	{
		const FVector Mid = (From + To) * 0.5f;
		const FVector Dir = (To - From).GetSafeNormal();
		const FVector Right = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();

		const float ArrowSize = 15.0f;
		PDI->DrawLine(Mid, Mid - Dir * ArrowSize + Right * ArrowSize * 0.5f, LineColor, SDPG_World, ConnectionLineThickness);
		PDI->DrawLine(Mid, Mid - Dir * ArrowSize - Right * ArrowSize * 0.5f, LineColor, SDPG_World, ConnectionLineThickness);
	}
}

void FPCGExValencyCageEditorMode::DrawOrbitalArrow(FPrimitiveDrawInterface* PDI, const FVector& Origin, const FVector& Direction, float Length, const FLinearColor& Color, bool bDashed)
{
	const FVector EndPoint = Origin + Direction * Length;

	if (bDashed)
	{
		// Draw dashed line (segments)
		const int32 NumSegments = 5;
		const float SegmentLength = Length / (NumSegments * 2);

		for (int32 i = 0; i < NumSegments; ++i)
		{
			const float Start = i * SegmentLength * 2;
			const float End = Start + SegmentLength;

			const FVector SegStart = Origin + Direction * Start;
			const FVector SegEnd = Origin + Direction * FMath::Min(End, Length);

			PDI->DrawLine(SegStart, SegEnd, Color, SDPG_World, 1.5f);
		}
	}
	else
	{
		PDI->DrawLine(Origin, EndPoint, Color, SDPG_World, 1.5f);
	}

	// Draw arrowhead
	const FVector Right = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
	const FVector Up = FVector::CrossProduct(Right, Direction).GetSafeNormal();
	const float ArrowSize = 10.0f;

	PDI->DrawLine(EndPoint, EndPoint - Direction * ArrowSize + Right * ArrowSize * 0.5f, Color, SDPG_World, 1.5f);
	PDI->DrawLine(EndPoint, EndPoint - Direction * ArrowSize - Right * ArrowSize * 0.5f, Color, SDPG_World, 1.5f);
	PDI->DrawLine(EndPoint, EndPoint - Direction * ArrowSize + Up * ArrowSize * 0.5f, Color, SDPG_World, 1.5f);
	PDI->DrawLine(EndPoint, EndPoint - Direction * ArrowSize - Up * ArrowSize * 0.5f, Color, SDPG_World, 1.5f);
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
