// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/PCGExTransform.h"

#include "PCGExDataMath.h"
#include "Engine/EngineTypes.h"

FAttachmentTransformRules FPCGExAttachmentRules::GetRules() const
{
	return FAttachmentTransformRules(LocationRule, RotationRule, ScaleRule, bWeldSimulatedBodies);
}

bool FPCGExUVW::Init(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade)
{
	UGetter = GetValueSettingU();
	if (!UGetter->Init(InDataFacade)) { return false; }

	VGetter = GetValueSettingV();
	if (!UGetter->Init(InDataFacade)) { return false; }

	WGetter = GetValueSettingW();
	if (!WGetter->Init(InDataFacade)) { return false; }

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

FPCGExAxisDeformDetails::FPCGExAxisDeformDetails(const FString InFirst, const FString InSecond, const double InFirstValue, const double InSecondValue)
{
	FirstAlphaAttribute = FName(TEXT("@Data.") + InFirst);
	FirstAlphaConstant = InFirstValue;

	SecondAlphaAttribute = FName(TEXT("@Data.") + InSecond);
	SecondAlphaConstant = InSecondValue;
}

bool FPCGExAxisDeformDetails::Validate(FPCGExContext* InContext, const bool bSupportPoints) const
{
	if (FirstAlphaInput != EPCGExSampleSource::Constant)
	{
		PCGEX_VALIDATE_NAME_C(InContext, FirstAlphaAttribute)
		if (!bSupportPoints && !PCGExHelpers::IsDataDomainAttribute(FirstAlphaAttribute))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT( "Only @Data attributes are supported."));
			PCGEX_LOG_INVALID_ATTR_C(InContext, First Alpha, FirstAlphaAttribute)
			return false;
		}
	}

	if (SecondAlphaInput != EPCGExSampleSource::Constant)
	{
		PCGEX_VALIDATE_NAME_C(InContext, SecondAlphaAttribute)
		if (!bSupportPoints && !PCGExHelpers::IsDataDomainAttribute(SecondAlphaAttribute))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT( "Only @Data attributes are supported."));
			PCGEX_LOG_INVALID_ATTR_C(InContext, Second Alpha, SecondAlphaAttribute)
			return false;
		}
	}

	return true;
}

bool FPCGExAxisDeformDetails::Init(FPCGExContext* InContext, const TArray<PCGExData::FTaggedData>& InTargets)
{
	if (FirstAlphaInput == EPCGExSampleSource::Target)
	{
		TargetsFirstValueGetter.Init(nullptr, InTargets.Num());
		for (int i = 0; i < InTargets.Num(); i++) { TargetsFirstValueGetter[i] = GetDataValueSettingFirstAlpha(InContext, InTargets[i].Data); }
	}

	if (SecondAlphaInput == EPCGExSampleSource::Target)
	{
		TargetsSecondValueGetter.Init(nullptr, InTargets.Num());
		for (int i = 0; i < InTargets.Num(); i++) { TargetsSecondValueGetter[i] = GetDataValueSettingSecondAlpha(InContext, InTargets[i].Data); }
	}

	return true;
}

bool FPCGExAxisDeformDetails::Init(FPCGExContext* InContext, const FPCGExAxisDeformDetails& Parent, const TSharedRef<PCGExData::FFacade>& InDataFacade, const int32 InTargetIndex, const bool bSupportPoint)
{
	if (Parent.FirstAlphaInput == EPCGExSampleSource::Target)
	{
		FirstValueGetter = Parent.TargetsFirstValueGetter[InTargetIndex];
	}
	else
	{
		if (Parent.FirstValueGetter)
		{
			FirstValueGetter = Parent.FirstValueGetter;
		}
		else
		{
			if (bSupportPoint)
			{
				FirstValueGetter = Parent.GetValueSettingFirstAlpha();
				if (!FirstValueGetter->Init(InDataFacade)) { return false; }
			}
			else
			{
				FirstValueGetter = Parent.GetDataValueSettingFirstAlpha(InContext, InDataFacade->GetIn());
			}
		}
	}

	if (Parent.SecondAlphaInput == EPCGExSampleSource::Target)
	{
		SecondValueGetter = Parent.TargetsSecondValueGetter[InTargetIndex];
	}
	else
	{
		if (Parent.SecondValueGetter)
		{
			SecondValueGetter = Parent.SecondValueGetter;
		}
		else
		{
			if (bSupportPoint)
			{
				SecondValueGetter = Parent.GetValueSettingSecondAlpha();
				if (!SecondValueGetter->Init(InDataFacade)) { return false; }
			}
			else
			{
				SecondValueGetter = Parent.GetDataValueSettingSecondAlpha(InContext, InDataFacade->GetIn());
			}
		}
	}

	return true;
}

void FPCGExAxisDeformDetails::GetAlphas(const int32 Index, double& OutFirst, double& OutSecond, const bool bSort) const
{
	OutFirst = FirstValueGetter->Read(Index);
	OutSecond = SecondValueGetter->Read(Index);

	if (Usage == EPCGExTransformAlphaUsage::CenterAndSize)
	{
		const double Center = OutFirst;
		OutFirst -= OutSecond;
		OutSecond += Center;
	}
	else if (Usage == EPCGExTransformAlphaUsage::StartAndSize)
	{
		OutSecond += OutFirst;
	}

	if (bSort && OutFirst > OutSecond) { std::swap(OutFirst, OutSecond); }
}


namespace PCGExTransform
{
	FBox GetBounds(const TArrayView<FVector> InPositions)
	{
		FBox Bounds = FBox(ForceInit);
		for (const FVector& Position : InPositions) { Bounds += Position; }
		SanitizeBounds(Bounds);
		return Bounds;
	}

	FBox GetBounds(const TConstPCGValueRange<FTransform>& InTransforms)
	{
		FBox Bounds = FBox(ForceInit);
		for (const FTransform& Transform : InTransforms) { Bounds += Transform.GetLocation(); }
		SanitizeBounds(Bounds);
		return Bounds;
	}

	FBox GetBounds(const UPCGBasePointData* InPointData, const EPCGExPointBoundsSource Source)
	{
		switch (Source)
		{
		case EPCGExPointBoundsSource::ScaledBounds:
			return GetBounds<EPCGExPointBoundsSource::ScaledBounds>(InPointData);
		case EPCGExPointBoundsSource::DensityBounds:
			return GetBounds<EPCGExPointBoundsSource::DensityBounds>(InPointData);
		case EPCGExPointBoundsSource::Bounds:
			return GetBounds<EPCGExPointBoundsSource::Bounds>(InPointData);
		default:
		case EPCGExPointBoundsSource::Center:
			return GetBounds<EPCGExPointBoundsSource::Center>(InPointData);
		}
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
}
