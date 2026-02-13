// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "EditorMode/PCGExValencyCageSocketVisualizer.h"

#include "EditorViewportClient.h"
#include "Editor.h"
#include "SceneManagement.h"
#include "EditorModeManager.h"

#include "PCGExValencyEditorSettings.h"
#include "Components/PCGExValencyCageSocketComponent.h"
#include "Cages/PCGExValencyCageBase.h"
#include "EditorMode/PCGExValencyCageEditorMode.h"

IMPLEMENT_HIT_PROXY(HPCGExSocketHitProxy, HComponentVisProxy);

#pragma region FPCGExValencyCageSocketVisualizer

void FPCGExValencyCageSocketVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	if (!Component || !PDI)
	{
		return;
	}

	const UPCGExValencyCageSocketComponent* SocketComp = Cast<UPCGExValencyCageSocketComponent>(Component);
	if (!SocketComp)
	{
		return;
	}

	const UPCGExValencyEditorSettings* Settings = UPCGExValencyEditorSettings::Get();
	if (!Settings || !Settings->bShowSocketVisualizers)
	{
		return;
	}

	// Check if Valency mode is active and socket visibility is enabled
	if (GLevelEditorModeTools().IsModeActive(UPCGExValencyCageEditorMode::ModeID))
	{
		if (const UPCGExValencyCageEditorMode* Mode = Cast<UPCGExValencyCageEditorMode>(
			GLevelEditorModeTools().GetActiveScriptableMode(UPCGExValencyCageEditorMode::ModeID)))
		{
			if (!Mode->GetVisibilityFlags().bShowSockets)
			{
				return;
			}
		}
	}

	// Get socket world transform
	const FTransform SocketTransform = SocketComp->GetComponentTransform();
	const FVector SocketLocation = SocketTransform.GetLocation();

	// Get effective debug color
	const APCGExValencyCageBase* OwnerCage = Cast<APCGExValencyCageBase>(SocketComp->GetOwner());
	const UPCGExValencySocketRules* SocketRules = OwnerCage ? OwnerCage->GetEffectiveSocketRules() : nullptr;
	FLinearColor Color = SocketComp->GetEffectiveDebugColor(SocketRules);

	// Disabled sockets: dimmed alpha
	if (!SocketComp->bEnabled)
	{
		Color.A *= Settings->SocketDisabledAlpha;
	}

	const float DiamondSize = Settings->SocketVisualizerSize;
	const float ArrowLength = Settings->SocketArrowLength;

	// Enqueue hit proxy for click detection
	PDI->SetHitProxy(new HPCGExSocketHitProxy(SocketComp));

	// Draw diamond at socket position
	DrawDiamond(PDI, SocketLocation, DiamondSize, Color, 2.0f);

	// Draw direction arrow along socket's forward axis
	const FVector Forward = SocketTransform.GetRotation().GetForwardVector();

	if (SocketComp->bIsOutputSocket)
	{
		// Output: arrow points outward (along forward)
		const FVector ArrowEnd = SocketLocation + Forward * ArrowLength;
		PDI->DrawLine(SocketLocation, ArrowEnd, Color, SDPG_Foreground, 1.5f);

		// Arrowhead
		const FVector Right = FVector::CrossProduct(Forward, FVector::UpVector).GetSafeNormal();
		const FVector Up = FVector::CrossProduct(Right, Forward).GetSafeNormal();
		const float HeadSize = DiamondSize * 0.5f;
		PDI->DrawLine(ArrowEnd, ArrowEnd - Forward * HeadSize + Right * HeadSize * 0.4f, Color, SDPG_Foreground, 1.5f);
		PDI->DrawLine(ArrowEnd, ArrowEnd - Forward * HeadSize - Right * HeadSize * 0.4f, Color, SDPG_Foreground, 1.5f);
		PDI->DrawLine(ArrowEnd, ArrowEnd - Forward * HeadSize + Up * HeadSize * 0.4f, Color, SDPG_Foreground, 1.5f);
		PDI->DrawLine(ArrowEnd, ArrowEnd - Forward * HeadSize - Up * HeadSize * 0.4f, Color, SDPG_Foreground, 1.5f);
	}
	else
	{
		// Input: arrow points inward (toward cage origin)
		const FVector ArrowStart = SocketLocation + Forward * ArrowLength;
		PDI->DrawLine(ArrowStart, SocketLocation, Color, SDPG_Foreground, 1.5f);

		// Arrowhead at socket location pointing inward
		const FVector InwardDir = -Forward;
		const FVector Right = FVector::CrossProduct(InwardDir, FVector::UpVector).GetSafeNormal();
		const FVector Up = FVector::CrossProduct(Right, InwardDir).GetSafeNormal();
		const float HeadSize = DiamondSize * 0.5f;
		PDI->DrawLine(SocketLocation, SocketLocation - InwardDir * HeadSize + Right * HeadSize * 0.4f, Color, SDPG_Foreground, 1.5f);
		PDI->DrawLine(SocketLocation, SocketLocation - InwardDir * HeadSize - Right * HeadSize * 0.4f, Color, SDPG_Foreground, 1.5f);
		PDI->DrawLine(SocketLocation, SocketLocation - InwardDir * HeadSize + Up * HeadSize * 0.4f, Color, SDPG_Foreground, 1.5f);
		PDI->DrawLine(SocketLocation, SocketLocation - InwardDir * HeadSize - Up * HeadSize * 0.4f, Color, SDPG_Foreground, 1.5f);
	}

	// Clear hit proxy
	PDI->SetHitProxy(nullptr);
}

bool FPCGExValencyCageSocketVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	if (!VisProxy)
	{
		return false;
	}

	const HPCGExSocketHitProxy* SocketProxy = HitProxyCast<HPCGExSocketHitProxy>(VisProxy);
	if (!SocketProxy || !SocketProxy->Component.IsValid())
	{
		return false;
	}

	UPCGExValencyCageSocketComponent* SocketComp = const_cast<UPCGExValencyCageSocketComponent*>(
		Cast<UPCGExValencyCageSocketComponent>(SocketProxy->Component.Get()));
	if (!SocketComp)
	{
		return false;
	}

	// Store selection for gizmo placement
	SelectedSocket = SocketComp;

	// Select the owning actor and highlight this component
	if (GEditor && SocketComp->GetOwner())
	{
		GEditor->SelectActor(SocketComp->GetOwner(), true, true);
		GEditor->SelectComponent(SocketComp, true, true);
	}

	return true;
}

bool FPCGExValencyCageSocketVisualizer::GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const
{
	if (SelectedSocket.IsValid())
	{
		OutLocation = SelectedSocket->GetComponentLocation();
		return true;
	}
	return false;
}

bool FPCGExValencyCageSocketVisualizer::HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale)
{
	if (!SelectedSocket.IsValid())
	{
		return false;
	}

	UPCGExValencyCageSocketComponent* SocketComp = SelectedSocket.Get();

	if (!DeltaTranslate.IsZero())
	{
		SocketComp->SetWorldLocation(SocketComp->GetComponentLocation() + DeltaTranslate);
	}

	if (!DeltaRotate.IsZero())
	{
		SocketComp->SetWorldRotation(SocketComp->GetComponentRotation() + DeltaRotate);
	}

	return true;
}

void FPCGExValencyCageSocketVisualizer::DrawDiamond(FPrimitiveDrawInterface* PDI, const FVector& Center, float Size, const FLinearColor& Color, float Thickness)
{
	// 3D diamond/rhombus: 6 vertices (top, bottom, front, back, left, right)
	const FVector Top = Center + FVector(0, 0, Size);
	const FVector Bottom = Center - FVector(0, 0, Size);
	const FVector Front = Center + FVector(Size, 0, 0);
	const FVector Back = Center - FVector(Size, 0, 0);
	const FVector Right = Center + FVector(0, Size, 0);
	const FVector Left = Center - FVector(0, Size, 0);

	// Top pyramid edges
	PDI->DrawLine(Top, Front, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Top, Back, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Top, Right, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Top, Left, Color, SDPG_Foreground, Thickness);

	// Bottom pyramid edges
	PDI->DrawLine(Bottom, Front, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Bottom, Back, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Bottom, Right, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Bottom, Left, Color, SDPG_Foreground, Thickness);

	// Equator edges
	PDI->DrawLine(Front, Right, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Right, Back, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Back, Left, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Left, Front, Color, SDPG_Foreground, Thickness);
}

#pragma endregion
