// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once
#include "PCGExDetails.h"
#include "Data/PCGExData.h"
#include "PCGExTransform.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExUVW
{
	GENERATED_BODY()

	FPCGExUVW()
	{
	}

	explicit FPCGExUVW(const double DefaultW)
		: WConstant(DefaultW)
	{
	}

	/** Overlap overlap test mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointBoundsSource BoundsReference = EPCGExPointBoundsSource::ScaledBounds;

	/** U Source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExInputValueType UInput = EPCGExInputValueType::Constant;

	/** U Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="U", EditCondition="UInput==EPCGExInputValueType::Constant", EditConditionHides, DisplayName="U"))
	double UConstant = 0;

	/** U Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="U", EditCondition="UInput==EPCGExInputValueType::Attribute", EditConditionHides, DisplayName="U"))
	FPCGAttributePropertyInputSelector UAttribute;

	/** V Source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExInputValueType VInput = EPCGExInputValueType::Constant;

	/** V Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="V", EditCondition="VInput==EPCGExInputValueType::Constant", EditConditionHides, DisplayName="V"))
	double VConstant = 0;

	/** V Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="V", EditCondition="VInput==EPCGExInputValueType::Attribute", EditConditionHides, DisplayName="V"))
	FPCGAttributePropertyInputSelector VAttribute;

	/** W Source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExInputValueType WInput = EPCGExInputValueType::Constant;

	/** W Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="W", EditCondition="WInput==EPCGExInputValueType::Constant", EditConditionHides, DisplayName="W"))
	double WConstant = 0;

	/** W Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="W", EditCondition="WInput==EPCGExInputValueType::Attribute", EditConditionHides, DisplayName="W"))
	FPCGAttributePropertyInputSelector WAttribute;

	TSharedPtr<PCGExData::TBuffer<double>> UGetter;
	TSharedPtr<PCGExData::TBuffer<double>> VGetter;
	TSharedPtr<PCGExData::TBuffer<double>> WGetter;

	bool Init(const FPCGContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade)
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

	// Without axis

	FVector GetUVW(const int32 PointIndex) const
	{
		return FVector(
			UGetter ? UGetter->Read(PointIndex) : UConstant,
			VGetter ? VGetter->Read(PointIndex) : VConstant,
			WGetter ? WGetter->Read(PointIndex) : WConstant);
	}

	FVector GetPosition(const PCGExData::FPointRef& PointRef) const
	{
		const FBox Bounds = PCGExMath::GetLocalBounds(*PointRef.Point, BoundsReference);
		const FVector LocalPosition = Bounds.GetCenter() + (Bounds.GetExtent() * GetUVW(PointRef.Index));
		return PointRef.Point->Transform.TransformPositionNoScale(LocalPosition);
	}

	FVector GetPosition(const PCGExData::FPointRef& PointRef, FVector& OutOffset) const
	{
		const FBox Bounds = PCGExMath::GetLocalBounds(*PointRef.Point, BoundsReference);
		const FVector LocalPosition = Bounds.GetCenter() + (Bounds.GetExtent() * GetUVW(PointRef.Index));
		OutOffset = PointRef.Point->Transform.TransformVectorNoScale(LocalPosition - Bounds.GetCenter());
		return PointRef.Point->Transform.TransformPositionNoScale(LocalPosition);
	}

	// With axis

	FVector GetUVW(const int32 PointIndex, const EPCGExMinimalAxis Axis, const bool bMirrorAxis = false) const
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

	FVector GetPosition(const PCGExData::FPointRef& PointRef, const EPCGExMinimalAxis Axis, const bool bMirrorAxis = false) const
	{
		const FBox Bounds = PCGExMath::GetLocalBounds(*PointRef.Point, BoundsReference);
		const FVector LocalPosition = Bounds.GetCenter() + (Bounds.GetExtent() * GetUVW(PointRef.Index, Axis, bMirrorAxis));
		return PointRef.Point->Transform.TransformPositionNoScale(LocalPosition);
	}

	FVector GetPosition(const PCGExData::FPointRef& PointRef, FVector& OutOffset, const EPCGExMinimalAxis Axis, const bool bMirrorAxis = false) const
	{
		const FBox Bounds = PCGExMath::GetLocalBounds(*PointRef.Point, BoundsReference);
		const FVector LocalPosition = Bounds.GetCenter() + (Bounds.GetExtent() * GetUVW(PointRef.Index, Axis, bMirrorAxis));
		OutOffset = PointRef.Point->Transform.TransformVectorNoScale(LocalPosition - Bounds.GetCenter());
		return PointRef.Point->Transform.TransformPositionNoScale(LocalPosition);
	}
};
