// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "EditorMode/Constraints/PCGExConstraintVis_SurfaceOffset.h"

#include "SceneManagement.h"
#include "Growth/Constraints/PCGExConstraint_SurfaceOffset.h"

#pragma region FSurfaceOffsetVisualizer

void FSurfaceOffsetVisualizer::DrawIndicator(
	FPrimitiveDrawInterface* PDI,
	const FTransform& ConnectorWorld,
	const FPCGExConnectorConstraint& Constraint,
	const FLinearColor& Color) const
{
	// Small square indicator near the connector
	const FVector Center = ConnectorWorld.GetTranslation();
	const FVector Right = ConnectorWorld.GetRotation().GetRightVector() * 3.0f;
	const FVector Up = ConnectorWorld.GetRotation().GetUpVector() * 3.0f;

	const FVector Offset = ConnectorWorld.GetRotation().GetUpVector() * 5.0f;
	PDI->DrawLine(Center + Offset - Right - Up, Center + Offset + Right - Up, Color, SDPG_World, 1.0f);
	PDI->DrawLine(Center + Offset + Right - Up, Center + Offset + Right + Up, Color, SDPG_World, 1.0f);
	PDI->DrawLine(Center + Offset + Right + Up, Center + Offset - Right + Up, Color, SDPG_World, 1.0f);
	PDI->DrawLine(Center + Offset - Right + Up, Center + Offset - Right - Up, Color, SDPG_World, 1.0f);
}

void FSurfaceOffsetVisualizer::DrawZone(
	FPrimitiveDrawInterface* PDI,
	const FTransform& ConnectorWorld,
	const FPCGExConnectorConstraint& Constraint,
	const FLinearColor& Color) const
{
	const auto& Surface = static_cast<const FPCGExConstraint_SurfaceOffset&>(Constraint);

	const FVector Center = ConnectorWorld.GetTranslation();
	const FQuat Rot = ConnectorWorld.GetRotation();
	const FVector Right = Rot.GetRightVector();
	const FVector Up = Rot.GetUpVector();

	const float HalfW = Surface.Width * 0.5f;
	const float HalfH = Surface.Height * 0.5f;

	// Four corners
	const FVector TL = Center + Right * (-HalfW) + Up * HalfH;
	const FVector TR = Center + Right * HalfW + Up * HalfH;
	const FVector BR = Center + Right * HalfW + Up * (-HalfH);
	const FVector BL = Center + Right * (-HalfW) + Up * (-HalfH);

	// Draw rectangle
	PDI->DrawLine(TL, TR, Color, SDPG_World, 1.0f);
	PDI->DrawLine(TR, BR, Color, SDPG_World, 1.0f);
	PDI->DrawLine(BR, BL, Color, SDPG_World, 1.0f);
	PDI->DrawLine(BL, TL, Color, SDPG_World, 1.0f);

	// Cross-hair at center
	PDI->DrawLine(Center + Right * (-3.0f), Center + Right * 3.0f, Color * 0.5f, SDPG_World, 0.5f);
	PDI->DrawLine(Center + Up * (-3.0f), Center + Up * 3.0f, Color * 0.5f, SDPG_World, 0.5f);
}

void FSurfaceOffsetVisualizer::DrawDetail(
	FPrimitiveDrawInterface* PDI,
	const FTransform& ConnectorWorld,
	const FPCGExConnectorConstraint& Constraint,
	const FLinearColor& Color,
	bool bIsActiveConstraint) const
{
	// Draw zone first
	DrawZone(PDI, ConnectorWorld, Constraint, Color);

	const auto& Surface = static_cast<const FPCGExConstraint_SurfaceOffset&>(Constraint);

	const FVector Center = ConnectorWorld.GetTranslation();
	const FQuat Rot = ConnectorWorld.GetRotation();
	const FVector Right = Rot.GetRightVector();
	const FVector Up = Rot.GetUpVector();

	const float HalfW = Surface.Width * 0.5f;
	const float HalfH = Surface.Height * 0.5f;

	// Draw corner handles as small dots
	const FLinearColor HandleColor = bIsActiveConstraint ? Color : Color * 0.8f;
	PDI->DrawPoint(Center + Right * (-HalfW) + Up * HalfH, HandleColor, 5.0f, SDPG_World);
	PDI->DrawPoint(Center + Right * HalfW + Up * HalfH, HandleColor, 5.0f, SDPG_World);
	PDI->DrawPoint(Center + Right * HalfW + Up * (-HalfH), HandleColor, 5.0f, SDPG_World);
	PDI->DrawPoint(Center + Right * (-HalfW) + Up * (-HalfH), HandleColor, 5.0f, SDPG_World);
}

#pragma endregion
