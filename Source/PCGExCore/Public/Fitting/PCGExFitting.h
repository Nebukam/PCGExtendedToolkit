// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFittingCommon.h"
#include "Data/PCGExDataCommon.h"
#include "Metadata/PCGAttributePropertySelector.h"

#include "PCGExFitting.generated.h"

struct FPCGExFittingVariations;

namespace PCGExData
{
}

struct FPCGExContext;

namespace PCGExData
{
	struct FPoint;
	class FFacade;

	template <typename T>
	class TBuffer;
}

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExScaleToFitDetails
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

	void Process(const PCGExData::FPoint& InPoint, const FBox& InBounds, FVector& OutScale, FBox& OutBounds) const;

private:
	static void ScaleToFitAxis(const EPCGExScaleToFit Fit, const int32 Axis, const FVector& TargetScale, const FVector& TargetSize, const FVector& CandidateSize, const FVector& MinMaxFit, FVector& OutScale);
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExSingleJustifyDetails
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

	void JustifyAxis(const int32 Axis, const int32 Index, const FVector& InCenter, const FVector& InSize, const FVector& OutCenter, const FVector& OutSize, FVector& OutTranslation) const;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExJustificationDetails
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

	void Process(const int32 Index, const FBox& InBounds, const FBox& OutBounds, FVector& OutTranslation) const;


	bool Init(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade);
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExFittingVariationsDetails
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

	void Apply(const FRandomStream& RandomStream, FTransform& OutTransform, const FPCGExFittingVariations& Variations, const EPCGExVariationMode& Step) const;
};


USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExFittingDetailsHandler
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
struct PCGEXCORE_API FPCGExTransformDetails : public FPCGExFittingDetailsHandler
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
struct PCGEXCORE_API FPCGExLeanTransformDetails
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
