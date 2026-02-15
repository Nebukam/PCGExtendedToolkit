// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "EditorMode/Constraints/PCGExConstraintVis_HemisphereOffset.h"

#include "SceneManagement.h"
#include "Growth/Constraints/PCGExConstraint_HemisphereOffset.h"

#pragma region FHemisphereOffsetVisualizer

void FHemisphereOffsetVisualizer::DrawIndicator(
	FPrimitiveDrawInterface* PDI,
	const FTransform& ConnectorWorld,
	const FPCGExConnectorConstraint& Constraint,
	const FLinearColor& Color) const
{
	// Small dome indicator - draw half-circle arc
	const FVector Center = ConnectorWorld.GetTranslation();
	const FVector Up = ConnectorWorld.GetRotation().GetUpVector();
	const FVector Forward = ConnectorWorld.GetRotation().GetForwardVector();

	const float S = 4.0f;
	const int32 Segments = 6;
	FVector Prev = Center + Up * S;
	for (int32 i = 1; i <= Segments; ++i)
	{
		const float Angle = PI * 0.5f * static_cast<float>(i) / static_cast<float>(Segments);
		const FVector Point = Center + Up * FMath::Cos(Angle) * S + Forward * FMath::Sin(Angle) * S;
		PDI->DrawLine(Prev, Point, Color, SDPG_World, 1.0f);
		Prev = Point;
	}
}

void FHemisphereOffsetVisualizer::DrawZone(
	FPrimitiveDrawInterface* PDI,
	const FTransform& ConnectorWorld,
	const FPCGExConnectorConstraint& Constraint,
	const FLinearColor& Color) const
{
	const auto& Hemi = static_cast<const FPCGExConstraint_HemisphereOffset&>(Constraint);

	const FVector Center = ConnectorWorld.GetTranslation();
	const FQuat Rot = ConnectorWorld.GetRotation();
	const FVector Forward = Rot.GetForwardVector();
	const FVector Right = Rot.GetRightVector();
	const FVector Up = Rot.GetUpVector();

	const float R = Hemi.Radius;
	const int32 ArcSegments = 16;

	// Draw 3 meridian arcs (hemisphere dome)
	// Arc in Forward-Right plane
	{
		FVector Prev = Center + Right * R;
		for (int32 i = 1; i <= ArcSegments; ++i)
		{
			const float Angle = PI * static_cast<float>(i) / static_cast<float>(ArcSegments);
			const FVector Point = Center + Right * FMath::Cos(Angle) * R + Forward * FMath::Sin(Angle) * R;
			PDI->DrawLine(Prev, Point, Color, SDPG_World, 0.5f);
			Prev = Point;
		}
	}

	// Arc in Forward-Up plane
	{
		FVector Prev = Center + Up * R;
		for (int32 i = 1; i <= ArcSegments; ++i)
		{
			const float Angle = PI * static_cast<float>(i) / static_cast<float>(ArcSegments);
			const FVector Point = Center + Up * FMath::Cos(Angle) * R + Forward * FMath::Sin(Angle) * R;
			PDI->DrawLine(Prev, Point, Color, SDPG_World, 0.5f);
			Prev = Point;
		}
	}

	// Equator circle (base of hemisphere) in Right-Up plane
	{
		FVector Prev = Center + Right * R;
		for (int32 i = 1; i <= ArcSegments * 2; ++i)
		{
			const float Angle = 2.0f * PI * static_cast<float>(i) / static_cast<float>(ArcSegments * 2);
			const FVector Point = Center + Right * FMath::Cos(Angle) * R + Up * FMath::Sin(Angle) * R;
			PDI->DrawLine(Prev, Point, Color, SDPG_World, 0.5f);
			Prev = Point;
		}
	}
}

void FHemisphereOffsetVisualizer::DrawDetail(
	FPrimitiveDrawInterface* PDI,
	const FTransform& ConnectorWorld,
	const FPCGExConnectorConstraint& Constraint,
	const FLinearColor& Color,
	bool bIsActiveConstraint) const
{
	// Draw zone first
	DrawZone(PDI, ConnectorWorld, Constraint, Color);

	const auto& Hemi = static_cast<const FPCGExConstraint_HemisphereOffset&>(Constraint);

	const FVector Center = ConnectorWorld.GetTranslation();
	const FVector Forward = ConnectorWorld.GetRotation().GetForwardVector();

	// Draw radius handle at dome tip
	const FLinearColor HandleColor = bIsActiveConstraint ? Color : Color * 0.8f;
	PDI->DrawPoint(Center + Forward * Hemi.Radius, HandleColor, 6.0f, SDPG_World);

	// Draw radius line
	PDI->DrawLine(Center, Center + Forward * Hemi.Radius, HandleColor * 0.5f, SDPG_World, 0.5f);
}

#pragma endregion
