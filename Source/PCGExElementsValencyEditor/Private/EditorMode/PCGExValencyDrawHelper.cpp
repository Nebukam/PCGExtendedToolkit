// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "EditorMode/PCGExValencyDrawHelper.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "SceneManagement.h"

#include "EditorMode/PCGExValencyEditorSettings.h"
#include "Cages/PCGExValencyCageBase.h"
#include "Cages/PCGExValencyCage.h"
#include "Cages/PCGExValencyCageOrbital.h"
#include "Cages/PCGExValencyAssetPalette.h"
#include "Volumes/ValencyContextVolume.h"
#include "Core/PCGExValencyOrbitalSet.h"

const UPCGExValencyEditorSettings* FPCGExValencyDrawHelper::GetSettings()
{
	return UPCGExValencyEditorSettings::Get();
}

void FPCGExValencyDrawHelper::DrawCage(FPrimitiveDrawInterface* PDI, const APCGExValencyCageBase* Cage)
{
	if (!PDI || !Cage)
	{
		return;
	}

	const UPCGExValencyEditorSettings* Settings = GetSettings();
	const FVector CageLocation = Cage->GetActorLocation();
	const FTransform CageTransform = Cage->GetActorTransform();
	const bool bIsNullCage = Cage->IsNullCage();

	// Null cages are valid without orbital sets - they're boundary markers
	// They have their own debug sphere component, no additional drawing needed
	if (bIsNullCage)
	{
		return;
	}

	// Draw mirror connections if this cage mirrors other actors
	if (const APCGExValencyCage* RegularCage = Cast<APCGExValencyCage>(Cage))
	{
		for (const TObjectPtr<AActor>& SourceActor : RegularCage->MirrorSources)
		{
			if (SourceActor)
			{
				DrawMirrorConnection(PDI, RegularCage, SourceActor);
			}
		}
	}

	// Get the orbital set for direction info
	const UPCGExValencyOrbitalSet* OrbitalSet = Cage->GetEffectiveOrbitalSet();

	// Non-null cages need an orbital set
	if (!OrbitalSet)
	{
		// Draw warning box if no orbital set
		DrawWireBox(PDI, FBox(CageLocation - FVector(25), CageLocation + FVector(25)), Settings->WarningColor, SDPG_World);
		return;
	}

	// Use probe radius for calculating arrow positions
	const float ProbeRadius = Cage->GetEffectiveProbeRadius();
	const float StartOffset = ProbeRadius * Settings->ArrowStartOffsetPct;

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
			const float ThinLineLength = ProbeRadius * Settings->ConnectedThinLinePct;

			FLinearColor ArrowColor;
			bool bDashed = false;
			bool bDrawArrowhead = true;
			FVector ArrowEnd;

			if (ConnectedCage->IsNullCage())
			{
				// Connection to null cage: dashed darkish red, no arrowhead on thick arrow
				ArrowColor = Settings->NullConnectionColor;
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
					ArrowColor = Settings->BidirectionalColor;
				}
				else
				{
					// Unilateral connection: teal with arrowhead
					ArrowColor = Settings->UnilateralColor;
				}
			}

			// Draw thin line along orbital direction (WorldDir), shorter length when connected
			DrawConnection(PDI, CageLocation, WorldDir, ThinLineLength, ArrowColor, false, false);

			// Draw the thick arrow - points toward connected cage
			DrawOrbitalArrow(PDI, ArrowStart, ArrowEnd, ArrowColor, bDashed, bDrawArrowhead);
		}
		else
		{
			// No connection: draw dashed thin line along orbital direction at full probe radius
			// with arrowhead to show direction
			DrawConnection(PDI, CageLocation, WorldDir, ProbeRadius, Settings->NoConnectionColor, true, true);
		}
	}
}

void FPCGExValencyDrawHelper::DrawVolume(FPrimitiveDrawInterface* PDI, const AValencyContextVolume* Volume)
{
	if (!PDI || !Volume)
	{
		return;
	}

	// Volumes handle their own brush rendering, but we can add overlay
	// Get volume bounds and draw with our color
	FVector Origin, BoxExtent;
	Volume->GetActorBounds(false, Origin, BoxExtent);

	const FBox VolumeBox(Origin - BoxExtent, Origin + BoxExtent);

	// Use volume's debug color if available
	const FLinearColor DrawColor = Volume->DebugColor;

	DrawWireBox(PDI, VolumeBox, DrawColor, SDPG_World);
}

void FPCGExValencyDrawHelper::DrawCageLabels(FCanvas* Canvas, const FSceneView* View, const APCGExValencyCageBase* Cage, bool bIsSelected)
{
	if (!Canvas || !View || !Cage)
	{
		return;
	}

	const UPCGExValencyEditorSettings* Settings = GetSettings();
	const FLinearColor LabelColor = bIsSelected ? Settings->SelectedLabelColor : Settings->UnselectedLabelColor;
	const FVector CageLocation = Cage->GetActorLocation();

	// Null cages: just show "NULL Cage" label, no orbitals
	if (Cage->IsNullCage())
	{
		DrawLabel(Canvas, View, CageLocation + FVector(0, 0, Settings->CageLabelVerticalOffset), TEXT("NULL Cage"), LabelColor);
		return;
	}

	// Draw cage name label
	const FString CageName = Cage->GetCageDisplayName();
	if (!CageName.IsEmpty())
	{
		DrawLabel(Canvas, View, CageLocation + FVector(0, 0, Settings->CageLabelVerticalOffset), CageName, LabelColor);
	}

	// Draw orbital labels if cage has an orbital set
	const UPCGExValencyOrbitalSet* OrbitalSet = Cage->GetEffectiveOrbitalSet();
	if (!OrbitalSet)
	{
		return;
	}

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

				// Position at configured percentage of probe radius
				const FVector LabelPos = CageLocation + WorldDir * (ProbeRadius * Settings->OrbitalLabelRadiusPct);
				DrawLabel(Canvas, View, LabelPos, Entry.GetDisplayName().ToString(), LabelColor);
			}
		}
	}
}

void FPCGExValencyDrawHelper::DrawLineSegment(FPrimitiveDrawInterface* PDI, const FVector& Start, const FVector& End, const FLinearColor& Color, float Thickness, bool bDashed)
{
	if (!PDI)
	{
		return;
	}

	if (bDashed)
	{
		const UPCGExValencyEditorSettings* Settings = GetSettings();
		const FVector Direction = (End - Start).GetSafeNormal();
		const float TotalLength = FVector::Dist(Start, End);
		const float DashCycle = Settings->DashLength + Settings->DashGap;
		float CurrentPos = 0.0f;

		while (CurrentPos < TotalLength)
		{
			const float DashStart = CurrentPos;
			const float DashEnd = FMath::Min(CurrentPos + Settings->DashLength, TotalLength);

			const FVector SegStart = Start + Direction * DashStart;
			const FVector SegEnd = Start + Direction * DashEnd;

			PDI->DrawLine(SegStart, SegEnd, Color, SDPG_World, Thickness);
			CurrentPos += DashCycle;
		}
	}
	else
	{
		PDI->DrawLine(Start, End, Color, SDPG_World, Thickness);
	}
}

void FPCGExValencyDrawHelper::DrawArrowhead(FPrimitiveDrawInterface* PDI, const FVector& TipLocation, const FVector& Direction, const FLinearColor& Color, float Size, float Thickness)
{
	if (!PDI)
	{
		return;
	}

	const FVector Right = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
	const FVector Up = FVector::CrossProduct(Right, Direction).GetSafeNormal();

	// Four prongs for 3D arrowhead
	PDI->DrawLine(TipLocation, TipLocation - Direction * Size + Right * Size * 0.5f, Color, SDPG_World, Thickness);
	PDI->DrawLine(TipLocation, TipLocation - Direction * Size - Right * Size * 0.5f, Color, SDPG_World, Thickness);
	PDI->DrawLine(TipLocation, TipLocation - Direction * Size + Up * Size * 0.5f, Color, SDPG_World, Thickness);
	PDI->DrawLine(TipLocation, TipLocation - Direction * Size - Up * Size * 0.5f, Color, SDPG_World, Thickness);
}

void FPCGExValencyDrawHelper::DrawConnection(FPrimitiveDrawInterface* PDI, const FVector& From, const FVector& Along, float Distance, const FLinearColor& Color, bool bDrawArrowhead, bool bDashed)
{
	if (!PDI)
	{
		return;
	}

	const UPCGExValencyEditorSettings* Settings = GetSettings();
	const FVector To = From + Along * Distance;
	DrawLineSegment(PDI, From, To, Color, Settings->ConnectionLineThickness, bDashed);

	if (bDrawArrowhead)
	{
		DrawArrowhead(PDI, To, Along, Color, Settings->ConnectionArrowheadSize, Settings->ConnectionLineThickness);
	}
}

void FPCGExValencyDrawHelper::DrawOrbitalArrow(FPrimitiveDrawInterface* PDI, const FVector& Start, const FVector& End, const FLinearColor& Color, bool bDashed, bool bDrawArrowhead)
{
	if (!PDI)
	{
		return;
	}

	const FVector Direction = (End - Start).GetSafeNormal();
	const float TotalLength = FVector::Dist(Start, End);

	if (TotalLength < KINDA_SMALL_NUMBER)
	{
		return;
	}

	const UPCGExValencyEditorSettings* Settings = GetSettings();

	// Arrow geometry when arrowhead is drawn:
	// - Main line up to configured percentage of length
	// - Arrowhead prongs at the end
	const float MainLinePct = bDrawArrowhead ? Settings->ArrowMainLinePct : 1.0f;
	const FVector MainLineEnd = Start + Direction * (TotalLength * MainLinePct);

	DrawLineSegment(PDI, Start, MainLineEnd, Color, Settings->OrbitalArrowThickness, bDashed);

	if (bDrawArrowhead)
	{
		DrawArrowhead(PDI, End, Direction, Color, Settings->ArrowheadSize, Settings->ArrowheadThickness);
	}
}

void FPCGExValencyDrawHelper::DrawLabel(FCanvas* Canvas, const FSceneView* View, const FVector& WorldLocation, const FString& Text, const FLinearColor& Color)
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

void FPCGExValencyDrawHelper::DrawPalette(FPrimitiveDrawInterface* PDI, const APCGExValencyAssetPalette* Palette)
{
	if (!PDI || !Palette)
	{
		return;
	}

	const FVector PaletteLocation = Palette->GetActorLocation();
	const FTransform PaletteTransform = Palette->GetActorTransform();
	const FLinearColor PaletteColor = Palette->PaletteColor;
	const FVector Extent = Palette->DetectionExtent;

	// Draw based on detection shape
	switch (Palette->DetectionShape)
	{
	case EPCGExAssetPaletteShape::Box:
		{
			// Draw wireframe box using palette transform
			const FBox LocalBox(-Extent, Extent);
			DrawWireBox(PDI, PaletteTransform.ToMatrixWithScale(), LocalBox, PaletteColor, SDPG_World);
		}
		break;

	case EPCGExAssetPaletteShape::Sphere:
		{
			// Draw wireframe sphere
			DrawWireSphere(PDI, PaletteLocation, PaletteColor, Extent.X, 16, SDPG_World);
		}
		break;

	case EPCGExAssetPaletteShape::Capsule:
		{
			// Draw wireframe capsule (radius = X, half-height = Z)
			DrawWireCapsule(PDI, PaletteLocation, FVector::ForwardVector, FVector::RightVector, FVector::UpVector,
				PaletteColor, Extent.X, Extent.Z + Extent.X, 16, SDPG_World);
		}
		break;
	}
}

void FPCGExValencyDrawHelper::DrawPaletteLabels(FCanvas* Canvas, const FSceneView* View, const APCGExValencyAssetPalette* Palette, bool bIsSelected)
{
	if (!Canvas || !View || !Palette)
	{
		return;
	}

	const UPCGExValencyEditorSettings* Settings = GetSettings();
	const FLinearColor LabelColor = bIsSelected ? Settings->SelectedLabelColor : Palette->PaletteColor;
	const FVector PaletteLocation = Palette->GetActorLocation();

	// Draw palette display name
	const FString PaletteName = Palette->GetPaletteDisplayName();
	if (!PaletteName.IsEmpty())
	{
		DrawLabel(Canvas, View, PaletteLocation + FVector(0, 0, Settings->CageLabelVerticalOffset), PaletteName, LabelColor);
	}
}

void FPCGExValencyDrawHelper::DrawMirrorConnection(FPrimitiveDrawInterface* PDI, const APCGExValencyCage* MirrorCage, const AActor* SourceActor)
{
	if (!PDI || !MirrorCage || !SourceActor)
	{
		return;
	}

	const UPCGExValencyEditorSettings* Settings = GetSettings();
	const FVector MirrorLocation = MirrorCage->GetActorLocation();
	const FVector SourceLocation = SourceActor->GetActorLocation();

	// Draw dashed line from mirror to source
	DrawLineSegment(PDI, MirrorLocation, SourceLocation, Settings->MirrorConnectionColor, Settings->OrbitalArrowThickness, true);

	// Draw a small diamond marker at the mirror cage to indicate it's a mirror
	const float MarkerSize = 15.0f;
	const FVector Up = FVector::UpVector * MarkerSize;
	const FVector Right = FVector::RightVector * MarkerSize;
	const FVector Forward = FVector::ForwardVector * MarkerSize;

	// Diamond shape around the cage
	PDI->DrawLine(MirrorLocation + Up, MirrorLocation + Right, Settings->MirrorConnectionColor, SDPG_World, Settings->ConnectionLineThickness);
	PDI->DrawLine(MirrorLocation + Right, MirrorLocation - Up, Settings->MirrorConnectionColor, SDPG_World, Settings->ConnectionLineThickness);
	PDI->DrawLine(MirrorLocation - Up, MirrorLocation - Right, Settings->MirrorConnectionColor, SDPG_World, Settings->ConnectionLineThickness);
	PDI->DrawLine(MirrorLocation - Right, MirrorLocation + Up, Settings->MirrorConnectionColor, SDPG_World, Settings->ConnectionLineThickness);

	// Vertical diamond
	PDI->DrawLine(MirrorLocation + Up, MirrorLocation + Forward, Settings->MirrorConnectionColor, SDPG_World, Settings->ConnectionLineThickness);
	PDI->DrawLine(MirrorLocation + Forward, MirrorLocation - Up, Settings->MirrorConnectionColor, SDPG_World, Settings->ConnectionLineThickness);
	PDI->DrawLine(MirrorLocation - Up, MirrorLocation - Forward, Settings->MirrorConnectionColor, SDPG_World, Settings->ConnectionLineThickness);
	PDI->DrawLine(MirrorLocation - Forward, MirrorLocation + Up, Settings->MirrorConnectionColor, SDPG_World, Settings->ConnectionLineThickness);
}
