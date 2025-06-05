// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/PCGExTransform.h"

#include "PCGExDataMath.h"

FAttachmentTransformRules FPCGExAttachmentRules::GetRules() const
{
	return FAttachmentTransformRules(LocationRule, RotationRule, ScaleRule, bWeldSimulatedBodies);
}

bool FPCGExUVW::Init(const FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade)
{
	UGetter = GetValueSettingU();
	if (!UGetter->Init(InContext, InDataFacade)) { return false; }

	VGetter = GetValueSettingV();
	if (!UGetter->Init(InContext, InDataFacade)) { return false; }

	WGetter = GetValueSettingW();
	if (!WGetter->Init(InContext, InDataFacade)) { return false; }

	PointData = InDataFacade->GetIn();
	if (!PointData) { return false; }

	return true;
}

FVector FPCGExUVW::GetPosition(const int32 PointIndex) const
{
	const FBox Bounds = PCGExMath::GetLocalBounds(PCGExData::FConstPoint(PointData, PointIndex), BoundsReference);
	const FVector LocalPosition = Bounds.GetCenter() + (Bounds.GetExtent() * GetUVW(PointIndex));
	return PointData->GetTransform(PointIndex).TransformPositionNoScale(LocalPosition);
}

FVector FPCGExUVW::GetPosition(const int32 PointIndex, FVector& OutOffset) const
{
	const FBox Bounds = PCGExMath::GetLocalBounds(PCGExData::FConstPoint(PointData, PointIndex), BoundsReference);
	const FVector LocalPosition = Bounds.GetCenter() + (Bounds.GetExtent() * GetUVW(PointIndex));
	const FTransform& Transform = PointData->GetTransform(PointIndex);
	OutOffset = Transform.TransformVectorNoScale(LocalPosition - Bounds.GetCenter());
	return Transform.TransformPositionNoScale(LocalPosition);
}

FVector FPCGExUVW::GetUVW(const int32 PointIndex, const EPCGExMinimalAxis Axis, const bool bMirrorAxis) const
{
	FVector Value = GetUVW(PointIndex);
	if (bMirrorAxis)
	{
		switch (Axis)
		{
		default: ;
		case EPCGExMinimalAxis::None:
			break;
		case EPCGExMinimalAxis::X:
			Value.X *= -1;
			break;
		case EPCGExMinimalAxis::Y:
			Value.Y *= -1;
			break;
		case EPCGExMinimalAxis::Z:
			Value.Z *= -1;
			break;
		}
	}
	return Value;
}

FVector FPCGExUVW::GetPosition(const int32 PointIndex, const EPCGExMinimalAxis Axis, const bool bMirrorAxis) const
{
	const FBox Bounds = PCGExMath::GetLocalBounds(PCGExData::FConstPoint(PointData, PointIndex), BoundsReference);
	const FVector LocalPosition = Bounds.GetCenter() + (Bounds.GetExtent() * GetUVW(PointIndex, Axis, bMirrorAxis));
	return PointData->GetTransform(PointIndex).TransformPositionNoScale(LocalPosition);
}

FVector FPCGExUVW::GetPosition(const int32 PointIndex, FVector& OutOffset, const EPCGExMinimalAxis Axis, const bool bMirrorAxis) const
{
	const FBox Bounds = PCGExMath::GetLocalBounds(PCGExData::FConstPoint(PointData, PointIndex), BoundsReference);
	const FVector LocalPosition = Bounds.GetCenter() + (Bounds.GetExtent() * GetUVW(PointIndex, Axis, bMirrorAxis));
	const FTransform& Transform = PointData->GetTransform(PointIndex);
	OutOffset = Transform.TransformVectorNoScale(LocalPosition - Bounds.GetCenter());
	return Transform.TransformPositionNoScale(LocalPosition);
}

FVector FPCGExConstantUVW::GetPosition(const PCGExData::FConstPoint& Point) const
{
	const FBox Bounds = PCGExMath::GetLocalBounds(Point, BoundsReference);
	const FVector LocalPosition = Bounds.GetCenter() + (Bounds.GetExtent() * FVector(U, V, W));
	return Point.GetTransform().TransformPositionNoScale(LocalPosition);
}

FVector FPCGExConstantUVW::GetPosition(const PCGExData::FConstPoint& Point, FVector& OutOffset) const
{
	const FBox Bounds = PCGExMath::GetLocalBounds(Point, BoundsReference);
	const FVector LocalPosition = Bounds.GetCenter() + (Bounds.GetExtent() * FVector(U, V, W));
	const FTransform& Transform = Point.GetTransform();
	OutOffset = Transform.TransformVectorNoScale(LocalPosition - Bounds.GetCenter());
	return Transform.TransformPositionNoScale(LocalPosition);
}

FVector FPCGExConstantUVW::GetUVW(const EPCGExMinimalAxis Axis, const bool bMirrorAxis) const
{
	FVector Value = FVector(U, V, W);
	if (bMirrorAxis)
	{
		switch (Axis)
		{
		default: ;
		case EPCGExMinimalAxis::None:
			break;
		case EPCGExMinimalAxis::X:
			Value.X *= -1;
			break;
		case EPCGExMinimalAxis::Y:
			Value.Y *= -1;
			break;
		case EPCGExMinimalAxis::Z:
			Value.Z *= -1;
			break;
		}
	}
	return Value;
}

FVector FPCGExConstantUVW::GetPosition(const PCGExData::FConstPoint& Point, const EPCGExMinimalAxis Axis, const bool bMirrorAxis) const
{
	const FBox Bounds = PCGExMath::GetLocalBounds(Point, BoundsReference);
	const FVector LocalPosition = Bounds.GetCenter() + (Bounds.GetExtent() * GetUVW(Axis, bMirrorAxis));
	return Point.GetTransform().TransformPositionNoScale(LocalPosition);
}

FVector FPCGExConstantUVW::GetPosition(const PCGExData::FConstPoint& Point, FVector& OutOffset, const EPCGExMinimalAxis Axis, const bool bMirrorAxis) const
{
	const FBox Bounds = PCGExMath::GetLocalBounds(Point, BoundsReference);
	const FVector LocalPosition = Bounds.GetCenter() + (Bounds.GetExtent() * GetUVW(Axis, bMirrorAxis));
	const FTransform& Transform = Point.GetTransform();
	OutOffset = Transform.TransformVectorNoScale(LocalPosition - Bounds.GetCenter());
	return Transform.TransformPositionNoScale(LocalPosition);
}
