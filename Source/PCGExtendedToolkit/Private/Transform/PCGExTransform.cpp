// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/PCGExTransform.h"
#include "PCGExMathBounds.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointElements.h"
#include "Details/PCGExDetailsSettings.h"
#include "Engine/EngineTypes.h"
#include "Sampling/PCGExSampling.h"


FPCGExAttachmentRules::FPCGExAttachmentRules(EAttachmentRule InLoc, EAttachmentRule InRot, EAttachmentRule InScale)
	: LocationRule(InLoc), RotationRule(InRot), ScaleRule(InScale)
{
}

FAttachmentTransformRules FPCGExAttachmentRules::GetRules() const
{
	return FAttachmentTransformRules(LocationRule, RotationRule, ScaleRule, bWeldSimulatedBodies);
}

FPCGExSocket::FPCGExSocket(const FName& InSocketName, const FVector& InRelativeLocation, const FRotator& InRelativeRotation, const FVector& InRelativeScale, FString InTag)
	: SocketName(InSocketName), RelativeTransform(FTransform(InRelativeRotation.Quaternion(), InRelativeLocation, InRelativeScale)), Tag(InTag)
{
}

FPCGExSocket::FPCGExSocket(const FName& InSocketName, const FTransform& InRelativeTransform, const FString& InTag)
	: SocketName(InSocketName), RelativeTransform(InRelativeTransform), Tag(InTag)
{
}

TSharedPtr<PCGExDetails::TSettingValue<FName>> FPCGExSocketFitDetails::GetValueSettingSocketName(const bool bQuietErrors) const
{
	TSharedPtr<PCGExDetails::TSettingValue<FName>> V = PCGExDetails::MakeSettingValue<FName>(SocketNameInput, SocketNameAttribute, SocketName);
	V->bQuietErrors = bQuietErrors;
	return V;
}

bool FPCGExSocketFitDetails::Init(const TSharedPtr<PCGExData::FFacade>& InFacade)
{
	if (!bEnabled ||
		(SocketNameInput == EPCGExInputValueType::Constant && SocketName.IsNone()) ||
		(SocketNameInput == EPCGExInputValueType::Attribute && SocketNameAttribute.IsNone()))
	{
		bMutate = false;
		return true;
	}

	bMutate = true;
	SocketNameBuffer = GetValueSettingSocketName();
	if (!SocketNameBuffer->Init(InFacade)) { return false; }

	return true;
}

void FPCGExSocketFitDetails::Mutate(const int32 Index, const TArray<FPCGExSocket>& InSockets, FTransform& InOutTransform) const
{
	if (!bMutate) { return; }

	const FName SName = SocketNameBuffer->Read(Index);
	for (const FPCGExSocket& Socket : InSockets)
	{
		if (Socket.SocketName == SName)
		{
			InOutTransform = InOutTransform * Socket.RelativeTransform;
			return;
		}
	}
}

TSharedPtr<PCGExDetails::TSettingValue<double>> FPCGExUVW::GetValueSettingU(const bool bQuietErrors) const
{
	TSharedPtr<PCGExDetails::TSettingValue<double>> V = PCGExDetails::MakeSettingValue<double>(UInput, UAttribute, UConstant);
	V->bQuietErrors = bQuietErrors;
	return V;
}

TSharedPtr<PCGExDetails::TSettingValue<double>> FPCGExUVW::GetValueSettingV(const bool bQuietErrors) const
{
	TSharedPtr<PCGExDetails::TSettingValue<double>> V = PCGExDetails::MakeSettingValue<double>(VInput, VAttribute, VConstant);
	V->bQuietErrors = bQuietErrors;
	return V;
}

TSharedPtr<PCGExDetails::TSettingValue<double>> FPCGExUVW::GetValueSettingW(const bool bQuietErrors) const
{
	TSharedPtr<PCGExDetails::TSettingValue<double>> V = PCGExDetails::MakeSettingValue<double>(WInput, WAttribute, WConstant);
	V->bQuietErrors = bQuietErrors;
	return V;
}

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
	OutOffset = (Bounds.GetExtent() * GetUVW(PointIndex));
	const FVector LocalPosition = Bounds.GetCenter() + OutOffset;
	return PointData->GetTransform(PointIndex).TransformPositionNoScale(LocalPosition);
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

TSharedPtr<PCGExDetails::TSettingValue<double>> FPCGExAxisDeformDetails::GetDataValueSettingFirstAlpha(FPCGExContext* InContext, const UPCGData* InData, const bool bQuietErrors) const
{
	TSharedPtr<PCGExDetails::TSettingValue<double>> V = PCGExDetails::MakeSettingValue<double>(InContext, InData, FirstAlphaInput != EPCGExSampleSource::Constant ? EPCGExInputValueType::Attribute : EPCGExInputValueType::Constant, FirstAlphaAttribute, FirstAlphaConstant);
	V->bQuietErrors = bQuietErrors;
	return V;
}

TSharedPtr<PCGExDetails::TSettingValue<double>> FPCGExAxisDeformDetails::GetValueSettingFirstAlpha(const bool bQuietErrors) const
{
	TSharedPtr<PCGExDetails::TSettingValue<double>> V = PCGExDetails::MakeSettingValue<double>(FirstAlphaInput != EPCGExSampleSource::Constant ? EPCGExInputValueType::Attribute : EPCGExInputValueType::Constant, FirstAlphaAttribute, FirstAlphaConstant);
	V->bQuietErrors = bQuietErrors;
	return V;
}

TSharedPtr<PCGExDetails::TSettingValue<double>> FPCGExAxisDeformDetails::GetDataValueSettingSecondAlpha(FPCGExContext* InContext, const UPCGData* InData, const bool bQuietErrors) const
{
	TSharedPtr<PCGExDetails::TSettingValue<double>> V = PCGExDetails::MakeSettingValue<double>(InContext, InData, SecondAlphaInput != EPCGExSampleSource::Constant ? EPCGExInputValueType::Attribute : EPCGExInputValueType::Constant, SecondAlphaAttribute, SecondAlphaConstant);
	V->bQuietErrors = bQuietErrors;
	return V;
}

TSharedPtr<PCGExDetails::TSettingValue<double>> FPCGExAxisDeformDetails::GetValueSettingSecondAlpha(const bool bQuietErrors) const
{
	TSharedPtr<PCGExDetails::TSettingValue<double>> V = PCGExDetails::MakeSettingValue<double>(SecondAlphaInput != EPCGExSampleSource::Constant ? EPCGExInputValueType::Attribute : EPCGExInputValueType::Constant, SecondAlphaAttribute, SecondAlphaConstant);
	V->bQuietErrors = bQuietErrors;
	return V;
}

bool FPCGExAxisDeformDetails::Validate(FPCGExContext* InContext, const bool bSupportPoints) const
{
	if (FirstAlphaInput != EPCGExSampleSource::Constant)
	{
		PCGEX_VALIDATE_NAME_C(InContext, FirstAlphaAttribute)
		if (!bSupportPoints && !PCGExHelpers::IsDataDomainAttribute(FirstAlphaAttribute))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Only @Data attributes are supported."));
			PCGEX_LOG_INVALID_ATTR_C(InContext, First Alpha, FirstAlphaAttribute)
			return false;
		}
	}

	if (SecondAlphaInput != EPCGExSampleSource::Constant)
	{
		PCGEX_VALIDATE_NAME_C(InContext, SecondAlphaAttribute)
		if (!bSupportPoints && !PCGExHelpers::IsDataDomainAttribute(SecondAlphaAttribute))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Only @Data attributes are supported."));
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
