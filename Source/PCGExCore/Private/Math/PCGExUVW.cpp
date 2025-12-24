// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Math/PCGExUVW.h"

#include "Math/PCGExMathBounds.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointElements.h"
#include "Details/PCGExSettingsDetails.h"
#include "Engine/EngineTypes.h"
#include "Math/PCGExMathAxis.h"

PCGEX_SETTING_VALUE_IMPL(FPCGExUVW, U, double, UInput, UAttribute, UConstant)
PCGEX_SETTING_VALUE_IMPL(FPCGExUVW, V, double, VInput, VAttribute, VConstant)
PCGEX_SETTING_VALUE_IMPL(FPCGExUVW, W, double, WInput, WAttribute, WConstant)

bool FPCGExUVW::Init(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade)
{
	UGetter = GetValueSettingU();
	if (!UGetter->Init(InDataFacade)) { return false; }

	VGetter = GetValueSettingV();
	if (!VGetter->Init(InDataFacade)) { return false; }

	WGetter = GetValueSettingW();
	if (!WGetter->Init(InDataFacade)) { return false; }

	PointData = InDataFacade->GetIn();
	if (!PointData) { return false; }

	return true;
}

FVector FPCGExUVW::GetUVW(const int32 PointIndex) const
{
	return FVector(UGetter->Read(PointIndex), VGetter->Read(PointIndex), WGetter->Read(PointIndex));
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
	OutOffset = Bounds.GetExtent() * GetUVW(PointIndex);
	return PointData->GetTransform(PointIndex).TransformPositionNoScale(Bounds.GetCenter() + OutOffset);
}

FVector FPCGExUVW::GetUVW(const int32 PointIndex, const EPCGExMinimalAxis Axis, const bool bMirrorAxis) const
{
	FVector Value = GetUVW(PointIndex);
	if (bMirrorAxis)
	{
		switch (Axis)
		{
		default: ;
		case EPCGExMinimalAxis::None: break;
		case EPCGExMinimalAxis::X: Value.X *= -1;
			break;
		case EPCGExMinimalAxis::Y: Value.Y *= -1;
			break;
		case EPCGExMinimalAxis::Z: Value.Z *= -1;
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
	OutOffset = (Bounds.GetExtent() * GetUVW(PointIndex, Axis, bMirrorAxis));
	return PointData->GetTransform(PointIndex).TransformPositionNoScale(Bounds.GetCenter() + OutOffset);
}

namespace PCGExMath
{
	FVector FPCGExConstantUVW::GetPosition(const PCGExData::FConstPoint& Point) const
	{
		const FBox Bounds = GetLocalBounds(Point, BoundsReference);
		const FVector LocalPosition = Bounds.GetCenter() + (Bounds.GetExtent() * FVector(U, V, W));
		return Point.GetTransform().TransformPositionNoScale(LocalPosition);
	}

	FVector FPCGExConstantUVW::GetPosition(const PCGExData::FConstPoint& Point, FVector& OutOffset) const
	{
		const FBox Bounds = GetLocalBounds(Point, BoundsReference);
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
			case EPCGExMinimalAxis::None: break;
			case EPCGExMinimalAxis::X: Value.X *= -1;
				break;
			case EPCGExMinimalAxis::Y: Value.Y *= -1;
				break;
			case EPCGExMinimalAxis::Z: Value.Z *= -1;
				break;
			}
		}
		return Value;
	}

	FVector FPCGExConstantUVW::GetPosition(const PCGExData::FConstPoint& Point, const EPCGExMinimalAxis Axis, const bool bMirrorAxis) const
	{
		const FBox Bounds = GetLocalBounds(Point, BoundsReference);
		const FVector LocalPosition = Bounds.GetCenter() + (Bounds.GetExtent() * GetUVW(Axis, bMirrorAxis));
		return Point.GetTransform().TransformPositionNoScale(LocalPosition);
	}

	FVector FPCGExConstantUVW::GetPosition(const PCGExData::FConstPoint& Point, FVector& OutOffset, const EPCGExMinimalAxis Axis, const bool bMirrorAxis) const
	{
		const FBox Bounds = GetLocalBounds(Point, BoundsReference);
		const FVector LocalPosition = Bounds.GetCenter() + (Bounds.GetExtent() * GetUVW(Axis, bMirrorAxis));
		const FTransform& Transform = Point.GetTransform();
		OutOffset = Transform.TransformVectorNoScale(LocalPosition - Bounds.GetCenter());
		return Transform.TransformPositionNoScale(LocalPosition);
	}
}
