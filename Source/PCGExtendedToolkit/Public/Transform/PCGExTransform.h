// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once
#include "PCGExDetails.h"
#include "PCGExDetailsData.h"
#include "Data/PCGExData.h"
#include "PCGExTransform.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAttachmentRules
{
	GENERATED_BODY()

	FPCGExAttachmentRules() = default;
	~FPCGExAttachmentRules() = default;

	/** The rule to apply to location when attaching */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EAttachmentRule LocationRule = EAttachmentRule::KeepWorld;

	/** The rule to apply to rotation when attaching */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EAttachmentRule RotationRule = EAttachmentRule::KeepWorld;

	/** The rule to apply to scale when attaching */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EAttachmentRule ScaleRule = EAttachmentRule::KeepWorld;

	/** Whether to weld simulated bodies together when attaching */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bWeldSimulatedBodies = false;

	FAttachmentTransformRules GetRules() const;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExUVW
{
	GENERATED_BODY()

	FPCGExUVW()
	{
	}

	explicit FPCGExUVW(const double DefaultW)
		: WConstant(DefaultW)
	{
	}

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointBoundsSource BoundsReference = EPCGExPointBoundsSource::ScaledBounds;

	/** U Source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExInputValueType UInput = EPCGExInputValueType::Constant;

	/** U Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="U (Attr)", EditCondition="UInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector UAttribute;

	/** U Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="U", EditCondition="UInput==EPCGExInputValueType::Constant", EditConditionHides))
	double UConstant = 0;

	PCGEX_SETTING_VALUE_GET(U, double, UInput, UAttribute, UConstant)

	/** V Source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExInputValueType VInput = EPCGExInputValueType::Constant;

	/** V Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="V (Attr)", EditCondition="VInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector VAttribute;

	/** V Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="V", EditCondition="VInput==EPCGExInputValueType::Constant", EditConditionHides))
	double VConstant = 0;

	PCGEX_SETTING_VALUE_GET(V, double, VInput, VAttribute, VConstant)

	/** W Source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExInputValueType WInput = EPCGExInputValueType::Constant;

	/** W Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="W (Attr)", EditCondition="WInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector WAttribute;

	/** W Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="W", EditCondition="WInput==EPCGExInputValueType::Constant", EditConditionHides))
	double WConstant = 0;

	PCGEX_SETTING_VALUE_GET(W, double, WInput, WAttribute, WConstant)

	TSharedPtr<PCGExDetails::TSettingValue<double>> UGetter;
	TSharedPtr<PCGExDetails::TSettingValue<double>> VGetter;
	TSharedPtr<PCGExDetails::TSettingValue<double>> WGetter;

	bool Init(const FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade);

	// Without axis

	FORCEINLINE FVector GetUVW(const int32 PointIndex) const { return FVector(UGetter->Read(PointIndex), VGetter->Read(PointIndex), WGetter->Read(PointIndex)); }

	FVector GetPosition(const PCGExData::FPointRef& PointRef) const;

	FVector GetPosition(const PCGExData::FPointRef& PointRef, FVector& OutOffset) const;

	// With axis

	FVector GetUVW(const int32 PointIndex, const EPCGExMinimalAxis Axis, const bool bMirrorAxis = false) const;

	FVector GetPosition(const PCGExData::FPointRef& PointRef, const EPCGExMinimalAxis Axis, const bool bMirrorAxis = false) const;

	FVector GetPosition(const PCGExData::FPointRef& PointRef, FVector& OutOffset, const EPCGExMinimalAxis Axis, const bool bMirrorAxis = false) const;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExConstantUVW
{
	GENERATED_BODY()

	FPCGExConstantUVW()
	{
	}

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointBoundsSource BoundsReference = EPCGExPointBoundsSource::ScaledBounds;

	/** U Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="U", EditCondition="UInput==EPCGExInputValueType::Constant", EditConditionHides))
	double U = 0;

	/** V Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="V", EditCondition="VInput==EPCGExInputValueType::Constant", EditConditionHides))
	double V = 0;

	/** W Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="W", EditCondition="WInput==EPCGExInputValueType::Constant", EditConditionHides))
	double W = 0;

	FORCEINLINE FVector GetUVW() const { return FVector(U, V, W); }

	FVector GetPosition(const PCGExData::FPointRef& PointRef) const;

	FVector GetPosition(const PCGExData::FPointRef& PointRef, FVector& OutOffset) const;

	// With axis

	FVector GetUVW(const EPCGExMinimalAxis Axis, const bool bMirrorAxis = false) const;

	FVector GetPosition(const PCGExData::FPointRef& PointRef, const EPCGExMinimalAxis Axis, const bool bMirrorAxis = false) const;

	FVector GetPosition(const PCGExData::FPointRef& PointRef, FVector& OutOffset, const EPCGExMinimalAxis Axis, const bool bMirrorAxis = false) const;
};
