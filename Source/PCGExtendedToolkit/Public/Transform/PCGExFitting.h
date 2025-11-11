// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "PCGExCommon.h"

#include "PCGExFitting.generated.h"

struct FPCGExContext;

namespace PCGExData
{
	struct FProxyPoint;
	class FFacade;
	struct FConstPoint;

	template <typename T>
	class TBuffer;
}

UENUM(BlueprintType)
enum class EPCGExFitMode : uint8
{
	None       = 0 UMETA(DisplayName = "None", ToolTip="No fitting", ActionIcon="STF_None"),
	Uniform    = 1 UMETA(DisplayName = "Uniform", ToolTip="Uniform fit", ActionIcon="STF_Uniform"),
	Individual = 2 UMETA(DisplayName = "Individual", ToolTip="Per-component fit", ActionIcon="STF_Individual"),
};

UENUM(BlueprintType)
enum class EPCGExScaleToFit : uint8
{
	None = 0 UMETA(DisplayName = "None", ToolTip="No fitting", ActionIcon="Fit_None"),
	Fill = 1 UMETA(DisplayName = "Fill", ToolTip="Fill", ActionIcon="Fit_Fill"),
	Min  = 2 UMETA(DisplayName = "Min", ToolTip="Min", ActionIcon="Fit_Min"),
	Max  = 3 UMETA(DisplayName = "Max", ToolTip="Max", ActionIcon="Fit_Max"),
	Avg  = 4 UMETA(DisplayName = "Average", ToolTip="Average", ActionIcon="Fit_Average"),
};

UENUM(BlueprintType)
enum class EPCGExJustifyFrom : uint8
{
	Min    = 0 UMETA(DisplayName = "Min", ToolTip="Min", ActionIcon="From_Min"),
	Center = 1 UMETA(DisplayName = "Center", ToolTip="Center", ActionIcon="From_Center"),
	Max    = 2 UMETA(DisplayName = "Max", ToolTip="Max", ActionIcon="From_Max"),
	Pivot  = 3 UMETA(DisplayName = "Pivot", ToolTip="Pivot", ActionIcon="From_Pivot"),
	Custom = 4 UMETA(DisplayName = "Custom", ToolTip="Custom", ActionIcon="From_Custom"),
};

UENUM(BlueprintType)
enum class EPCGExJustifyTo : uint8
{
	Min    = 1 UMETA(DisplayName = "Min", ToolTip="Min", ActionIcon="To_Min"),
	Center = 2 UMETA(DisplayName = "Center", ToolTip="Center", ActionIcon="To_Center"),
	Max    = 3 UMETA(DisplayName = "Max", ToolTip="Max", ActionIcon="To_Max"),
	Pivot  = 4 UMETA(DisplayName = "Pivot", ToolTip="Pivot", ActionIcon="To_Pivot"),
	Custom = 5 UMETA(DisplayName = "Custom", ToolTip="Custom", ActionIcon="To_Custom"),
	Same   = 0 UMETA(DisplayName = "Same", ToolTip="Same as 'From'", ActionIcon="To_Same"),
};

UENUM(BlueprintType)
enum class EPCGExVariationMode : uint8
{
	Disabled = 0 UMETA(DisplayName = "Disabled", ToolTip="..."),
	Before   = 1 UMETA(DisplayName = "Before fitting", ToolTip="Variation are applied to the point that will be fitted"),
	After    = 2 UMETA(DisplayName = "After fitting", ToolTip="Variation are applied to the fitted bounds"),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExScaleToFitDetails
{
	GENERATED_BODY()

	FPCGExScaleToFitDetails()
	{
	}

	explicit FPCGExScaleToFitDetails(const EPCGExFitMode DefaultFit)
	{
		ScaleToFitMode = DefaultFit;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFitMode ScaleToFitMode = EPCGExFitMode::Uniform;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="ScaleToFitMode == EPCGExFitMode::Uniform", EditConditionHides))
	EPCGExScaleToFit ScaleToFit = EPCGExScaleToFit::Min;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="ScaleToFitMode == EPCGExFitMode::Individual", EditConditionHides))
	EPCGExScaleToFit ScaleToFitX = EPCGExScaleToFit::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="ScaleToFitMode == EPCGExFitMode::Individual", EditConditionHides))
	EPCGExScaleToFit ScaleToFitY = EPCGExScaleToFit::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="ScaleToFitMode == EPCGExFitMode::Individual", EditConditionHides))
	EPCGExScaleToFit ScaleToFitZ = EPCGExScaleToFit::None;

	void Process(
		const PCGExData::FConstPoint& InPoint,
		const FBox& InBounds,
		FVector& OutScale,
		FBox& OutBounds) const;

private:
	static void ScaleToFitAxis(
		const EPCGExScaleToFit Fit, const int32 Axis,
		const FVector& TargetScale, const FVector& TargetSize,
		const FVector& CandidateSize, const FVector& MinMaxFit,
		FVector& OutScale);
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSingleJustifyDetails
{
	GENERATED_BODY()

	FPCGExSingleJustifyDetails();

	/** Reference point inside the bounds getting justified */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExJustifyFrom From = EPCGExJustifyFrom::Center;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName=" ├─ Input", EditCondition="From == EPCGExJustifyFrom::Custom", EditConditionHides))
	EPCGExInputValueType FromInput = EPCGExInputValueType::Constant;

	/**  Value is expected to be 0-1 normalized, 0 being bounds min and 1 being bounds min + size. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ From (Attr)", EditCondition="From == EPCGExJustifyFrom::Custom && FromInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector FromSourceAttribute;

	/**  Value is expected to be 0-1 normalized, 0 being bounds min and 1 being bounds min + size. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ From", EditCondition="From == EPCGExJustifyFrom::Custom && FromInput == EPCGExInputValueType::Constant", EditConditionHides))
	double FromConstant = 0.5;

	TSharedPtr<PCGExData::TBuffer<double>> FromGetter;
	TSharedPtr<PCGExData::TBuffer<FVector>> SharedFromGetter;

	/** Reference point inside the container bounds*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExJustifyTo To = EPCGExJustifyTo::Same;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName=" ├─ Input", EditCondition="To == EPCGExJustifyTo::Custom", EditConditionHides))
	EPCGExInputValueType ToInput = EPCGExInputValueType::Constant;

	/**  Value is expected to be 0-1 normalized, 0 being bounds min and 1 being bounds min + size. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ To (Attr)", EditCondition="To == EPCGExJustifyTo::Custom && ToInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector ToSourceAttribute;

	/**  Value is expected to be 0-1 normalized, 0 being bounds min and 1 being bounds min + size. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ To", EditCondition="To == EPCGExJustifyTo::Custom && ToInput == EPCGExInputValueType::Constant", EditConditionHides))
	double ToConstant = 0.5;

	TSharedPtr<PCGExData::TBuffer<double>> ToGetter;
	TSharedPtr<PCGExData::TBuffer<FVector>> SharedToGetter;

	bool Init(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade);

	void JustifyAxis(
		const int32 Axis,
		const int32 Index,
		const FVector& InCenter,
		const FVector& InSize,
		const FVector& OutCenter,
		const FVector& OutSize,
		FVector& OutTranslation) const;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExJustificationDetails
{
	GENERATED_BODY()

	FPCGExJustificationDetails()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bDoJustifyX = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bDoJustifyX"))
	FPCGExSingleJustifyDetails JustifyX;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bDoJustifyY = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bDoJustifyY"))
	FPCGExSingleJustifyDetails JustifyY;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bDoJustifyZ = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bDoJustifyZ"))
	FPCGExSingleJustifyDetails JustifyZ;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bSharedCustomFromAttribute = false;

	/**  Whether to use matching component of this Vector attribute for custom 'From' justifications instead of setting local attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bSharedCustomFromAttribute"))
	FPCGAttributePropertyInputSelector CustomFromVectorAttribute;

	TSharedPtr<PCGExData::TBuffer<FVector>> SharedFromGetter;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bSharedCustomToAttribute = false;

	/**  Whether to use matching component of this Vector attribute for custom 'To' justifications instead of setting local attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bSharedCustomToAttribute"))
	FPCGAttributePropertyInputSelector CustomToVectorAttribute;

	TSharedPtr<PCGExData::TBuffer<FVector>> SharedToGetter;

	void Process(
		const int32 Index,
		const FBox& InBounds,
		const FBox& OutBounds,
		FVector& OutTranslation) const;


	bool Init(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade);
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExFittingVariations
{
	GENERATED_BODY()

	/** Min/max variation on position, applied as an offset from the original position. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector OffsetMin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector OffsetMax = FVector::ZeroVector;

	/** Set offset in world space */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bAbsoluteOffset = false;

	/** Min/max variation on rotation, applied as an offset from the original rotation, unless specified otherwise in the dropdown */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FRotator RotationMin = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FRotator RotationMax = FRotator::ZeroRotator;

	/** Set rotation directly instead of additively on the selected axis */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditConditionHides, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExAbsoluteRotationFlags"))
	uint8 AbsoluteRotation = 0;

	/** Min/max variation on scale, applied as a multiplicative offset from the original scale. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (AllowPreserveRatio, PCG_Overridable))
	FVector ScaleMin = FVector::One();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (AllowPreserveRatio, PCG_Overridable))
	FVector ScaleMax = FVector::One();

	/** Scale uniformly on each axis. Uses the X component of ScaleMin and ScaleMax. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bUniformScale = true;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExFittingVariationsDetails
{
	GENERATED_BODY()

	FPCGExFittingVariationsDetails()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExVariationMode Offset = EPCGExVariationMode::Disabled;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExVariationMode Rotation = EPCGExVariationMode::Disabled;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExVariationMode Scale = EPCGExVariationMode::Disabled;

	bool bEnabledBefore = true;
	bool bEnabledAfter = true;

	int Seed = 0;

	void Init(const int InSeed);

	void Apply(
		int32 BaseSeed,
		FTransform& OutTransform,
		const FPCGExFittingVariations& Variations, const EPCGExVariationMode& Step) const;
};


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExFittingDetailsHandler
{
	GENERATED_BODY()

	explicit FPCGExFittingDetailsHandler()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExScaleToFitDetails ScaleToFit;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExJustificationDetails Justification;

	TSharedPtr<PCGExData::FFacade> TargetDataFacade;

	bool Init(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InTargetFacade);

	void ComputeTransform(const int32 TargetIndex, FTransform& OutTransform, FBox& InOutBounds, const bool bWorldSpace = true) const;
	void ComputeLocalTransform(const int32 TargetIndex, const FTransform& InLocalXForm, FTransform& OutTransform, FBox& InOutBounds) const;

	bool WillChangeBounds() const;
	bool WillChangeTransform() const;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExTransformDetails : public FPCGExFittingDetailsHandler
{
	GENERATED_BODY()

	explicit FPCGExTransformDetails()
	{
	}

	explicit FPCGExTransformDetails(const bool InInheritScale, const bool InInheritRotation)
		: bInheritScale(InInheritScale), bInheritRotation(InInheritRotation)
	{
	}

	/** If enabled, copied point will be scaled by the target' scale. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayAfter="Justification"))
	bool bInheritScale = false;

	/** If enabled, copied points will be rotated by the target' rotation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayAfter="bInheritScale"))
	bool bInheritRotation = false;

	/** If enabled, ignore bounds in calculations and only use position. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayAfter="bInheritRotation"))
	bool bIgnoreBounds = false;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExLeanTransformDetails
{
	GENERATED_BODY()

	explicit FPCGExLeanTransformDetails()
	{
	}

	/** If enabled, point will be scaled by the parent' scale. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInheritScale = true;

	/** If enabled, points will be rotated by the parent' rotation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInheritRotation = true;
};
