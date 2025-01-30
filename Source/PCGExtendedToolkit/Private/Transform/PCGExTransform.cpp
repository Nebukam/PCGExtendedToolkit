// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/PCGExTransform.h"

FAttachmentTransformRules FPCGExAttachmentRules::GetRules() const
{
	return FAttachmentTransformRules(LocationRule, RotationRule, ScaleRule, bWeldSimulatedBodies);
}

bool FPCGExUVW::Init(const FPCGContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade)
{
	if (UInput == EPCGExInputValueType::Attribute)
	{
		UGetter = InDataFacade->GetScopedBroadcaster<double>(UAttribute);
		if (!UGetter)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::FromString(TEXT("Invalid attribute for U.")));
			return false;
		}
	}

	if (VInput == EPCGExInputValueType::Attribute)
	{
		VGetter = InDataFacade->GetScopedBroadcaster<double>(VAttribute);
		if (!VGetter)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::FromString(TEXT("Invalid attribute for V.")));
			return false;
		}
	}

	if (WInput == EPCGExInputValueType::Attribute)
	{
		WGetter = InDataFacade->GetScopedBroadcaster<double>(WAttribute);
		if (!WGetter)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::FromString(TEXT("Invalid attribute for W.")));
			return false;
		}
	}

	return true;
}

FVector FPCGExUVW::GetUVW(const int32 PointIndex) const
{
	return FVector(
		UGetter ? UGetter->Read(PointIndex) : UConstant,
		VGetter ? VGetter->Read(PointIndex) : VConstant,
		WGetter ? WGetter->Read(PointIndex) : WConstant);
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
