// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/PCGExTransform.h"

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

	return true;
}

FVector FPCGExUVW::GetPosition(const PCGExData::FPointRef& PointRef) const
{
	const FBox Bounds = PCGExMath::GetLocalBounds(*PointRef.Point, BoundsReference);
	const FVector LocalPosition = Bounds.GetCenter() + (Bounds.GetExtent() * GetUVW(PointRef.Index));
	return PointRef.Point->Transform.TransformPositionNoScale(LocalPosition);
}

FVector FPCGExUVW::GetPosition(const PCGExData::FPointRef& PointRef, FVector& OutOffset) const
{
	const FBox Bounds = PCGExMath::GetLocalBounds(*PointRef.Point, BoundsReference);
	const FVector LocalPosition = Bounds.GetCenter() + (Bounds.GetExtent() * GetUVW(PointRef.Index));
	OutOffset = PointRef.Point->Transform.TransformVectorNoScale(LocalPosition - Bounds.GetCenter());
	return PointRef.Point->Transform.TransformPositionNoScale(LocalPosition);
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

FVector FPCGExUVW::GetPosition(const PCGExData::FPointRef& PointRef, const EPCGExMinimalAxis Axis, const bool bMirrorAxis) const
{
	const FBox Bounds = PCGExMath::GetLocalBounds(*PointRef.Point, BoundsReference);
	const FVector LocalPosition = Bounds.GetCenter() + (Bounds.GetExtent() * GetUVW(PointRef.Index, Axis, bMirrorAxis));
	return PointRef.Point->Transform.TransformPositionNoScale(LocalPosition);
}

FVector FPCGExUVW::GetPosition(const PCGExData::FPointRef& PointRef, FVector& OutOffset, const EPCGExMinimalAxis Axis, const bool bMirrorAxis) const
{
	const FBox Bounds = PCGExMath::GetLocalBounds(*PointRef.Point, BoundsReference);
	const FVector LocalPosition = Bounds.GetCenter() + (Bounds.GetExtent() * GetUVW(PointRef.Index, Axis, bMirrorAxis));
	OutOffset = PointRef.Point->Transform.TransformVectorNoScale(LocalPosition - Bounds.GetCenter());
	return PointRef.Point->Transform.TransformPositionNoScale(LocalPosition);
}

FVector FPCGExConstantUVW::GetPosition(const PCGExData::FPointRef& PointRef) const
{
	const FBox Bounds = PCGExMath::GetLocalBounds(*PointRef.Point, BoundsReference);
	const FVector LocalPosition = Bounds.GetCenter() + (Bounds.GetExtent() * FVector(U, V, W));
	return PointRef.Point->Transform.TransformPositionNoScale(LocalPosition);
}

FVector FPCGExConstantUVW::GetPosition(const PCGExData::FPointRef& PointRef, FVector& OutOffset) const
{
	const FBox Bounds = PCGExMath::GetLocalBounds(*PointRef.Point, BoundsReference);
	const FVector LocalPosition = Bounds.GetCenter() + (Bounds.GetExtent() * FVector(U, V, W));
	OutOffset = PointRef.Point->Transform.TransformVectorNoScale(LocalPosition - Bounds.GetCenter());
	return PointRef.Point->Transform.TransformPositionNoScale(LocalPosition);
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

FVector FPCGExConstantUVW::GetPosition(const PCGExData::FPointRef& PointRef, const EPCGExMinimalAxis Axis, const bool bMirrorAxis) const
{
	const FBox Bounds = PCGExMath::GetLocalBounds(*PointRef.Point, BoundsReference);
	const FVector LocalPosition = Bounds.GetCenter() + (Bounds.GetExtent() * GetUVW(Axis, bMirrorAxis));
	return PointRef.Point->Transform.TransformPositionNoScale(LocalPosition);
}

FVector FPCGExConstantUVW::GetPosition(const PCGExData::FPointRef& PointRef, FVector& OutOffset, const EPCGExMinimalAxis Axis, const bool bMirrorAxis) const
{
	const FBox Bounds = PCGExMath::GetLocalBounds(*PointRef.Point, BoundsReference);
	const FVector LocalPosition = Bounds.GetCenter() + (Bounds.GetExtent() * GetUVW(Axis, bMirrorAxis));
	OutOffset = PointRef.Point->Transform.TransformVectorNoScale(LocalPosition - Bounds.GetCenter());
	return PointRef.Point->Transform.TransformPositionNoScale(LocalPosition);
}
