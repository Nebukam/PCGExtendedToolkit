// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "EditorMode/PCGExValencyCageConnectorVisualizer.h"

#include "EditorViewportClient.h"
#include "Editor.h"
#include "SceneManagement.h"
#include "EditorModeManager.h"

#include "PCGExValencyEditorSettings.h"
#include "Selection.h"
#include "Components/PCGExValencyCageConnectorComponent.h"
#include "Cages/PCGExValencyCageBase.h"
#include "EditorMode/PCGExValencyCageEditorMode.h"

IMPLEMENT_HIT_PROXY(HPCGExConnectorHitProxy, HComponentVisProxy);

#pragma region FPCGExValencyCageConnectorVisualizer

void FPCGExValencyCageConnectorVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	if (!Component || !PDI)
	{
		return;
	}

	const UPCGExValencyCageConnectorComponent* ConnectorComp = Cast<UPCGExValencyCageConnectorComponent>(Component);
	if (!ConnectorComp)
	{
		return;
	}

	const UPCGExValencyEditorSettings* Settings = UPCGExValencyEditorSettings::Get();
	if (!Settings || !Settings->bShowConnectorVisualizers)
	{
		return;
	}

	// Check if Valency mode is active and connector visibility is enabled
	if (GLevelEditorModeTools().IsModeActive(UPCGExValencyCageEditorMode::ModeID))
	{
		if (const UPCGExValencyCageEditorMode* Mode = Cast<UPCGExValencyCageEditorMode>(
			GLevelEditorModeTools().GetActiveScriptableMode(UPCGExValencyCageEditorMode::ModeID)))
		{
			if (!Mode->GetVisibilityFlags().bShowConnectors)
			{
				return;
			}
		}
	}

	const FTransform ConnectorTransform = ConnectorComp->GetComponentTransform();
	const FVector ConnectorLocation = ConnectorTransform.GetLocation();

	const APCGExValencyCageBase* OwnerCage = Cast<APCGExValencyCageBase>(ConnectorComp->GetOwner());
	const UPCGExValencyConnectorSet* ConnectorSet = OwnerCage ? OwnerCage->GetEffectiveConnectorSet() : nullptr;
	FLinearColor Color = ConnectorComp->GetEffectiveDebugColor(ConnectorSet);

	if (!ConnectorComp->bEnabled)
	{
		Color.A *= Settings->ConnectorDisabledAlpha;
	}

	const float DiamondSize = Settings->ConnectorVisualizerSize;
	const float ArrowLength = Settings->ConnectorArrowLength;

	PDI->SetHitProxy(new HPCGExConnectorHitProxy(ConnectorComp));

	// Draw diamond at connector position
	DrawDiamond(PDI, ConnectorLocation, DiamondSize, Color, 2.0f);

	// Draw polarity-aware arrow
	const FVector Forward = ConnectorTransform.GetRotation().GetForwardVector();

	if (ConnectorComp->Polarity == EPCGExConnectorPolarity::Plug)
	{
		// Plug: arrow points outward (along forward)
		const FVector ArrowEnd = ConnectorLocation + Forward * ArrowLength;
		PDI->DrawLine(ConnectorLocation, ArrowEnd, Color, SDPG_Foreground, 1.5f);

		const FVector Right = FVector::CrossProduct(Forward, FVector::UpVector).GetSafeNormal();
		const FVector Up = FVector::CrossProduct(Right, Forward).GetSafeNormal();
		const float HeadSize = DiamondSize * 0.5f;
		PDI->DrawLine(ArrowEnd, ArrowEnd - Forward * HeadSize + Right * HeadSize * 0.4f, Color, SDPG_Foreground, 1.5f);
		PDI->DrawLine(ArrowEnd, ArrowEnd - Forward * HeadSize - Right * HeadSize * 0.4f, Color, SDPG_Foreground, 1.5f);
		PDI->DrawLine(ArrowEnd, ArrowEnd - Forward * HeadSize + Up * HeadSize * 0.4f, Color, SDPG_Foreground, 1.5f);
		PDI->DrawLine(ArrowEnd, ArrowEnd - Forward * HeadSize - Up * HeadSize * 0.4f, Color, SDPG_Foreground, 1.5f);
	}
	else if (ConnectorComp->Polarity == EPCGExConnectorPolarity::Port)
	{
		// Port: arrow points inward (toward diamond center)
		const FVector ArrowStart = ConnectorLocation + Forward * ArrowLength;
		PDI->DrawLine(ArrowStart, ConnectorLocation, Color, SDPG_Foreground, 1.5f);

		const FVector InwardDir = -Forward;
		const FVector Right = FVector::CrossProduct(InwardDir, FVector::UpVector).GetSafeNormal();
		const FVector Up = FVector::CrossProduct(Right, InwardDir).GetSafeNormal();
		const float HeadSize = DiamondSize * 0.5f;
		PDI->DrawLine(ConnectorLocation, ConnectorLocation - InwardDir * HeadSize + Right * HeadSize * 0.4f, Color, SDPG_Foreground, 1.5f);
		PDI->DrawLine(ConnectorLocation, ConnectorLocation - InwardDir * HeadSize - Right * HeadSize * 0.4f, Color, SDPG_Foreground, 1.5f);
		PDI->DrawLine(ConnectorLocation, ConnectorLocation - InwardDir * HeadSize + Up * HeadSize * 0.4f, Color, SDPG_Foreground, 1.5f);
		PDI->DrawLine(ConnectorLocation, ConnectorLocation - InwardDir * HeadSize - Up * HeadSize * 0.4f, Color, SDPG_Foreground, 1.5f);
	}
	// Universal: Diamond only (no arrow) - represents bidirectional

	PDI->SetHitProxy(nullptr);
}

bool FPCGExValencyCageConnectorVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	if (!VisProxy)
	{
		return false;
	}

	const HPCGExConnectorHitProxy* ConnectorProxy = HitProxyCast<HPCGExConnectorHitProxy>(VisProxy);
	if (!ConnectorProxy || !ConnectorProxy->Component.IsValid())
	{
		return false;
	}

	UPCGExValencyCageConnectorComponent* ConnectorComp = const_cast<UPCGExValencyCageConnectorComponent*>(
		Cast<UPCGExValencyCageConnectorComponent>(ConnectorProxy->Component.Get()));
	if (!ConnectorComp)
	{
		return false;
	}

	if (GEditor && ConnectorComp->GetOwner())
	{
		GEditor->SelectActor(ConnectorComp->GetOwner(), true, true);
		GEditor->SelectComponent(ConnectorComp, true, true);
	}

	return true;
}

bool FPCGExValencyCageConnectorVisualizer::GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const
{
	// Derive widget location from editor selection — clears when actor is deselected
	if (UPCGExValencyCageConnectorComponent* Connector = UPCGExValencyCageEditorMode::GetSelectedConnector())
	{
		// Only show widget if the owning actor is still selected
		AActor* OwnerActor = Connector->GetOwner();
		if (GEditor && OwnerActor && GEditor->GetSelectedActors()->IsSelected(OwnerActor))
		{
			OutLocation = Connector->GetComponentLocation();
			return true;
		}
	}
	return false;
}

bool FPCGExValencyCageConnectorVisualizer::HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale)
{
	UPCGExValencyCageConnectorComponent* ConnectorComp = UPCGExValencyCageEditorMode::GetSelectedConnector();
	if (!ConnectorComp)
	{
		return false;
	}

	// Only allow manipulation if the owning actor is still selected
	AActor* OwnerActor = ConnectorComp->GetOwner();
	if (!GEditor || !OwnerActor || !GEditor->GetSelectedActors()->IsSelected(OwnerActor))
	{
		return false;
	}

	if (!DeltaTranslate.IsZero())
	{
		ConnectorComp->SetWorldLocation(ConnectorComp->GetComponentLocation() + DeltaTranslate);
	}

	if (!DeltaRotate.IsZero())
	{
		ConnectorComp->SetWorldRotation(ConnectorComp->GetComponentRotation() + DeltaRotate);
	}

	return true;
}

void FPCGExValencyCageConnectorVisualizer::EndEditing()
{
	// Selection-driven — nothing to clear
}

void FPCGExValencyCageConnectorVisualizer::DrawDiamond(FPrimitiveDrawInterface* PDI, const FVector& Center, float Size, const FLinearColor& Color, float Thickness)
{
	const FVector Top = Center + FVector(0, 0, Size);
	const FVector Bottom = Center - FVector(0, 0, Size);
	const FVector Front = Center + FVector(Size, 0, 0);
	const FVector Back = Center - FVector(Size, 0, 0);
	const FVector Right = Center + FVector(0, Size, 0);
	const FVector Left = Center - FVector(0, Size, 0);

	PDI->DrawLine(Top, Front, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Top, Back, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Top, Right, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Top, Left, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Bottom, Front, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Bottom, Back, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Bottom, Right, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Bottom, Left, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Front, Right, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Right, Back, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Back, Left, Color, SDPG_Foreground, Thickness);
	PDI->DrawLine(Left, Front, Color, SDPG_Foreground, Thickness);
}

#pragma endregion
