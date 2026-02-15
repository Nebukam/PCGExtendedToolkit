// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "EditorMode/Constraints/PCGExConstraintVis_AngularRange.h"

#include "SceneManagement.h"
#include "Math/PCGExMathAxis.h"
#include "Growth/Constraints/PCGExConstraint_AngularRange.h"

namespace
{
	/** Get the two perpendicular plane vectors for drawing arcs around the given axis */
	void GetArcPlaneVectors(const FQuat& Rot, EPCGExAxis Axis, FVector& OutRight, FVector& OutUp)
	{
		const FVector Forward = Rot.GetForwardVector();
		const FVector Right = Rot.GetRightVector();
		const FVector Up = Rot.GetUpVector();

		switch (Axis)
		{
		default:
		case EPCGExAxis::Forward:
		case EPCGExAxis::Backward:
			OutRight = Right;
			OutUp = Up;
			break;
		case EPCGExAxis::Right:
		case EPCGExAxis::Left:
			OutRight = Forward;
			OutUp = Up;
			break;
		case EPCGExAxis::Up:
		case EPCGExAxis::Down:
			OutRight = Forward;
			OutUp = Right;
			break;
		}
	}

	/** Draw an arc segment around an axis */
	void DrawArc(FPrimitiveDrawInterface* PDI, const FVector& Center,
		const FVector& PlaneRight, const FVector& PlaneUp, float Radius,
		float MinAngle, float MaxAngle, int32 NumSegments,
		const FLinearColor& Color, float Thickness)
	{
		const float AngleRange = MaxAngle - MinAngle;
		FVector PrevPoint = Center + (FMath::Cos(FMath::DegreesToRadians(MinAngle)) * PlaneRight + FMath::Sin(FMath::DegreesToRadians(MinAngle)) * PlaneUp) * Radius;

		for (int32 i = 1; i <= NumSegments; ++i)
		{
			const float Angle = MinAngle + AngleRange * static_cast<float>(i) / static_cast<float>(NumSegments);
			const FVector Point = Center + (FMath::Cos(FMath::DegreesToRadians(Angle)) * PlaneRight + FMath::Sin(FMath::DegreesToRadians(Angle)) * PlaneUp) * Radius;
			PDI->DrawLine(PrevPoint, Point, Color, SDPG_World, Thickness);
			PrevPoint = Point;
		}
	}
}

#pragma region FAngularRangeVisualizer

void FAngularRangeVisualizer::DrawIndicator(
	FPrimitiveDrawInterface* PDI,
	const FTransform& ConnectorWorld,
	const FPCGExConnectorConstraint& Constraint,
	const FLinearColor& Color) const
{
	// Small dot near the connector position
	const FVector Location = ConnectorWorld.GetTranslation() + ConnectorWorld.GetRotation().GetUpVector() * 5.0f;
	PDI->DrawPoint(Location, Color, 6.0f, SDPG_World);
}

void FAngularRangeVisualizer::DrawZone(
	FPrimitiveDrawInterface* PDI,
	const FTransform& ConnectorWorld,
	const FPCGExConnectorConstraint& Constraint,
	const FLinearColor& Color) const
{
	const auto& Angular = static_cast<const FPCGExConstraint_AngularRange&>(Constraint);

	const FVector Center = ConnectorWorld.GetTranslation();
	const FQuat Rot = ConnectorWorld.GetRotation();

	FVector PlaneRight, PlaneUp;
	GetArcPlaneVectors(Rot, Angular.RotationAxis, PlaneRight, PlaneUp);

	const float MinAngle = Angular.GetMinAngle();
	const float MaxAngle = Angular.GetMaxAngle();

	const float ArcRadius = 15.0f;
	const int32 ArcSegments = FMath::Max(16, Angular.Steps * 4);

	// Draw arc around selected axis
	DrawArc(PDI, Center, PlaneRight, PlaneUp, ArcRadius,
		MinAngle, MaxAngle, ArcSegments, Color, 1.0f);

	// Draw radial lines at min and max angles
	const FVector MinDir = FMath::Cos(FMath::DegreesToRadians(MinAngle)) * PlaneRight + FMath::Sin(FMath::DegreesToRadians(MinAngle)) * PlaneUp;
	const FVector MaxDir = FMath::Cos(FMath::DegreesToRadians(MaxAngle)) * PlaneRight + FMath::Sin(FMath::DegreesToRadians(MaxAngle)) * PlaneUp;
	PDI->DrawLine(Center, Center + MinDir * ArcRadius, Color * 0.6f, SDPG_World, 0.5f);
	PDI->DrawLine(Center, Center + MaxDir * ArcRadius, Color * 0.6f, SDPG_World, 0.5f);

	// Draw short axis indicator line
	const FVector AxisDir = PCGExMath::GetDirection(Rot, Angular.RotationAxis);
	PDI->DrawLine(Center, Center + AxisDir * 8.0f, Color * 0.4f, SDPG_World, 0.5f);
}

void FAngularRangeVisualizer::DrawDetail(
	FPrimitiveDrawInterface* PDI,
	const FTransform& ConnectorWorld,
	const FPCGExConnectorConstraint& Constraint,
	const FLinearColor& Color,
	bool bIsActiveConstraint) const
{
	// Draw zone first
	DrawZone(PDI, ConnectorWorld, Constraint, Color);

	const auto& Angular = static_cast<const FPCGExConstraint_AngularRange&>(Constraint);

	const FVector Center = ConnectorWorld.GetTranslation();
	const FQuat Rot = ConnectorWorld.GetRotation();

	FVector PlaneRight, PlaneUp;
	GetArcPlaneVectors(Rot, Angular.RotationAxis, PlaneRight, PlaneUp);

	const float MinAngle = Angular.GetMinAngle();
	const float ArcRadius = 15.0f;
	const float TickLength = 4.0f;
	const float Range = Angular.HalfWidthDegrees * 2.0f;
	const float StepSize = (Angular.Steps > 1) ? (Range / static_cast<float>(Angular.Steps)) : 0.0f;

	// Draw step tick marks
	const FLinearColor TickColor = bIsActiveConstraint ? Color : Color * 0.8f;
	for (int32 i = 0; i <= Angular.Steps; ++i)
	{
		const float Angle = MinAngle + StepSize * static_cast<float>(i);
		const FVector Dir = FMath::Cos(FMath::DegreesToRadians(Angle)) * PlaneRight + FMath::Sin(FMath::DegreesToRadians(Angle)) * PlaneUp;
		const FVector Inner = Center + Dir * (ArcRadius - TickLength);
		const FVector Outer = Center + Dir * (ArcRadius + TickLength);
		PDI->DrawLine(Inner, Outer, TickColor, SDPG_World, 1.5f);
	}

	// Draw center angle indicator
	const float CenterAngleRad = FMath::DegreesToRadians(Angular.CenterAngleDegrees);
	const FVector CenterDir = FMath::Cos(CenterAngleRad) * PlaneRight + FMath::Sin(CenterAngleRad) * PlaneUp;
	PDI->DrawLine(Center, Center + CenterDir * (ArcRadius + TickLength * 2.0f), TickColor, SDPG_World, 1.0f);
	PDI->DrawPoint(Center + CenterDir * (ArcRadius + TickLength * 2.0f), TickColor, 5.0f, SDPG_World);
}

#pragma endregion
