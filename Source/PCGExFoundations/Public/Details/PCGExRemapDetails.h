// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once
#include "CoreMinimal.h"
#include "Details/PCGExInputShorthandsDetails.h"
#include "Fitting/PCGExFittingCommon.h"
#include "Math/PCGExMath.h"
#include "Sampling/PCGExSamplingCommon.h"
#include "Utils/PCGExCurveLookup.h"
#include "Curves/CurveFloat.h"
#include "Curves/RichCurve.h"

#include "PCGExRemapDetails.generated.h"

USTRUCT(BlueprintType)
struct PCGEXFOUNDATIONS_API FPCGExRemapDetails
{
	GENERATED_BODY()

	FPCGExRemapDetails()
	{
		LocalScoreCurve.EditorCurveData.AddKey(0, 0);
		LocalScoreCurve.EditorCurveData.AddKey(1, 1);
	}

	~FPCGExRemapDetails()
	{
	}

	/** Whether or not to use only positive values to compute range.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	bool bUseAbsoluteRange = true;

	/** Whether or not to preserve value sign when using absolute range.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, EditCondition="bUseAbsoluteRange"))
	bool bPreserveSign = true;

	/** Fixed In Min value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bUseInMin = false;

	/** Fixed In Min value. If disabled, will use the lowest input value.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bUseInMin"))
	double InMin = 0;

	/** Fixed In Max value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle, FullyExpand=true))
	bool bUseInMax = false;

	/** Fixed In Max value. If disabled, will use the highest input value.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bUseInMax"))
	double InMax = 0;

	/** How to remap before sampling the curve. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExRangeType RangeMethod = EPCGExRangeType::EffectiveRange;

	/** Scale output value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double Scale = 1;

	/** Whether to use in-editor curve or an external asset. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	bool bUseLocalCurve = false;

	// TODO: DirtyCache for OnDependencyChanged when this float curve is an external asset
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, DisplayName="Remap Curve", EditCondition = "bUseLocalCurve", EditConditionHides))
	FRuntimeFloatCurve LocalScoreCurve;

	UPROPERTY(EditAnywhere, Category = Settings, BlueprintReadWrite, meta =(PCG_Overridable, DisplayName="Remap Curve", EditCondition = "!bUseLocalCurve", EditConditionHides))
	TSoftObjectPtr<UCurveFloat> RemapCurve = TSoftObjectPtr<UCurveFloat>(PCGExCurves::WeightDistributionLinear);

	PCGExFloatLUT RemapLUT = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	FPCGExCurveLookupDetails RemapCurveLookup;

	/** Whether and how to truncate output value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExTruncateMode TruncateOutput = EPCGExTruncateMode::None;

	/** Scale the value after it's been truncated. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="TruncateOutput != EPCGExTruncateMode::None", EditConditionHides))
	double PostTruncateScale = 1;

	/** Offset applied to the component after remap. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double Offset = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExVariationSnapping Snapping = EPCGExVariationSnapping::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Snapping != EPCGExVariationSnapping::None", EditConditionHides))
	FPCGExInputShorthandSelectorDouble Snap = FPCGExInputShorthandSelectorDouble(FName("Step"), 10, false);

	void Init();

	double GetRemappedValue(const double Value, const double Step) const;
};
