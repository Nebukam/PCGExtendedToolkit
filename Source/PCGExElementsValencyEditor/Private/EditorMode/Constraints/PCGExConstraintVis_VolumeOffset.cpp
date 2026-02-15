// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "EditorMode/Constraints/PCGExConstraintVis_VolumeOffset.h"

#include "SceneManagement.h"
#include "Growth/Constraints/PCGExConstraint_VolumeOffset.h"

#pragma region FVolumeOffsetVisualizer

void FVolumeOffsetVisualizer::DrawIndicator(
	FPrimitiveDrawInterface* PDI,
	const FTransform& ConnectorWorld,
	const FPCGExConnectorConstraint& Constraint,
	const FLinearColor& Color) const
{
	// Small cube indicator
	const FVector Center = ConnectorWorld.GetTranslation() + ConnectorWorld.GetRotation().GetUpVector() * 5.0f;
	const float S = 2.5f;
	const FBox SmallBox(Center - FVector(S), Center + FVector(S));
	DrawWireBox(PDI, SmallBox, Color.ToFColor(true), SDPG_World);
}

void FVolumeOffsetVisualizer::DrawZone(
	FPrimitiveDrawInterface* PDI,
	const FTransform& ConnectorWorld,
	const FPCGExConnectorConstraint& Constraint,
	const FLinearColor& Color) const
{
	const auto& Volume = static_cast<const FPCGExConstraint_VolumeOffset&>(Constraint);

	const FVector Center = ConnectorWorld.GetTranslation();
	const FQuat Rot = ConnectorWorld.GetRotation();
	const FVector Forward = Rot.GetForwardVector();
	const FVector Right = Rot.GetRightVector();
	const FVector Up = Rot.GetUpVector();

	// Build 8 corners in connector-local space
	const FVector E = Volume.Extent;
	const FVector Corners[8] = {
		Center + Forward * (-E.X) + Right * (-E.Y) + Up * (-E.Z),
		Center + Forward * E.X + Right * (-E.Y) + Up * (-E.Z),
		Center + Forward * E.X + Right * E.Y + Up * (-E.Z),
		Center + Forward * (-E.X) + Right * E.Y + Up * (-E.Z),
		Center + Forward * (-E.X) + Right * (-E.Y) + Up * E.Z,
		Center + Forward * E.X + Right * (-E.Y) + Up * E.Z,
		Center + Forward * E.X + Right * E.Y + Up * E.Z,
		Center + Forward * (-E.X) + Right * E.Y + Up * E.Z,
	};

	// Bottom face
	PDI->DrawLine(Corners[0], Corners[1], Color, SDPG_World, 0.5f);
	PDI->DrawLine(Corners[1], Corners[2], Color, SDPG_World, 0.5f);
	PDI->DrawLine(Corners[2], Corners[3], Color, SDPG_World, 0.5f);
	PDI->DrawLine(Corners[3], Corners[0], Color, SDPG_World, 0.5f);
	// Top face
	PDI->DrawLine(Corners[4], Corners[5], Color, SDPG_World, 0.5f);
	PDI->DrawLine(Corners[5], Corners[6], Color, SDPG_World, 0.5f);
	PDI->DrawLine(Corners[6], Corners[7], Color, SDPG_World, 0.5f);
	PDI->DrawLine(Corners[7], Corners[4], Color, SDPG_World, 0.5f);
	// Pillars
	PDI->DrawLine(Corners[0], Corners[4], Color, SDPG_World, 0.5f);
	PDI->DrawLine(Corners[1], Corners[5], Color, SDPG_World, 0.5f);
	PDI->DrawLine(Corners[2], Corners[6], Color, SDPG_World, 0.5f);
	PDI->DrawLine(Corners[3], Corners[7], Color, SDPG_World, 0.5f);
}

void FVolumeOffsetVisualizer::DrawDetail(
	FPrimitiveDrawInterface* PDI,
	const FTransform& ConnectorWorld,
	const FPCGExConnectorConstraint& Constraint,
	const FLinearColor& Color,
	bool bIsActiveConstraint) const
{
	// Draw zone first
	DrawZone(PDI, ConnectorWorld, Constraint, Color);

	const auto& Volume = static_cast<const FPCGExConstraint_VolumeOffset&>(Constraint);

	const FVector Center = ConnectorWorld.GetTranslation();
	const FQuat Rot = ConnectorWorld.GetRotation();
	const FVector Forward = Rot.GetForwardVector();
	const FVector Right = Rot.GetRightVector();
	const FVector Up = Rot.GetUpVector();

	// Draw face-center handles
	const FLinearColor HandleColor = bIsActiveConstraint ? Color : Color * 0.8f;
	PDI->DrawPoint(Center + Forward * Volume.Extent.X, HandleColor, 5.0f, SDPG_World);
	PDI->DrawPoint(Center - Forward * Volume.Extent.X, HandleColor, 5.0f, SDPG_World);
	PDI->DrawPoint(Center + Right * Volume.Extent.Y, HandleColor, 5.0f, SDPG_World);
	PDI->DrawPoint(Center - Right * Volume.Extent.Y, HandleColor, 5.0f, SDPG_World);
	PDI->DrawPoint(Center + Up * Volume.Extent.Z, HandleColor, 5.0f, SDPG_World);
	PDI->DrawPoint(Center - Up * Volume.Extent.Z, HandleColor, 5.0f, SDPG_World);
}

#pragma endregion
