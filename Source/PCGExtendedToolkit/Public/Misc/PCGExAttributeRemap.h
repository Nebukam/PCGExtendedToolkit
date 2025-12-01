// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Curves/CurveFloat.h"
#include "Curves/RichCurve.h"

#include "PCGEx.h"
#include "PCGExGlobalSettings.h"
#include "PCGExMath.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExProxyData.h"
#include "Details/PCGExDetailsAttributes.h"
#include "Details/PCGExDetailsInputShorthands.h"
#include "Sampling/PCGExSampling.h"
#include "Transform/PCGExFitting.h"

#include "PCGExAttributeRemap.generated.h"

namespace PCGExMT
{
	template <typename T>
	class TScopedNumericValue;
}

namespace PCGExData
{
	class IBufferProxy;
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExClampDetails
{
	GENERATED_BODY()

	FPCGExClampDetails()
	{
	}

	FPCGExClampDetails(const FPCGExClampDetails& Other):
		bApplyClampMin(Other.bApplyClampMin),
		ClampMinValue(Other.ClampMinValue),
		bApplyClampMax(Other.bApplyClampMax),
		ClampMaxValue(Other.ClampMaxValue)
	{
	}

	/** Clamp minimum value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bApplyClampMin = false;

	/** Clamp minimum value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bApplyClampMin"))
	double ClampMinValue = 0;

	/** Clamp maximum value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bApplyClampMax = false;

	/** Clamp maximum value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bApplyClampMax"))
	double ClampMaxValue = 0;

	FORCEINLINE double GetClampMin(const double InValue) const { return InValue < ClampMinValue ? ClampMinValue : InValue; }
	FORCEINLINE double GetClampMax(const double InValue) const { return InValue > ClampMaxValue ? ClampMaxValue : InValue; }
	FORCEINLINE double GetClampMinMax(const double InValue) const { return InValue > ClampMaxValue ? ClampMaxValue : InValue < ClampMinValue ? ClampMinValue : InValue; }

	FORCEINLINE double GetClampedValue(const double InValue) const
	{
		if (bApplyClampMin && InValue < ClampMinValue) { return ClampMinValue; }
		if (bApplyClampMax && InValue > ClampMaxValue) { return ClampMaxValue; }
		return InValue;
	}
};


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExRemapDetails
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
	TSoftObjectPtr<UCurveFloat> RemapCurve = TSoftObjectPtr<UCurveFloat>(PCGEx::WeightDistributionLinear);

	const FRichCurve* RemapCurveObj = nullptr;

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

	void Init()
	{
		if (!bUseLocalCurve) { LocalScoreCurve.ExternalCurve = RemapCurve.Get(); }
		RemapCurveObj = LocalScoreCurve.GetRichCurveConst();
	}

	double GetRemappedValue(const double Value, const double Step) const;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExComponentRemapRule
{
	GENERATED_BODY()

	FPCGExComponentRemapRule()
	{
	}

	FPCGExComponentRemapRule(const FPCGExComponentRemapRule& Other):
		InputClampDetails(Other.InputClampDetails),
		RemapDetails(Other.RemapDetails),
		OutputClampDetails(Other.OutputClampDetails)
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Clamp Input"))
	FPCGExClampDetails InputClampDetails;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExRemapDetails RemapDetails;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Clamp Output"))
	FPCGExClampDetails OutputClampDetails;

	TSharedPtr<PCGExMT::TScopedNumericValue<double>> MinCache;
	TSharedPtr<PCGExMT::TScopedNumericValue<double>> MaxCache;
	TSharedPtr<PCGExDetails::TSettingValue<double>> SnapCache;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="metadata/attribute-remap"))
class UPCGExAttributeRemapSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	virtual void ApplyDeprecation(UPCGNode* InOutNode) override;

	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(AttributeRemap, "Attribute Remap", "Remap a single property or attribute.", FName(GetDisplayName()));
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->ColorMiscWrite); }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	virtual PCGExData::EIOInit GetMainDataInitializationPolicy() const override;

public:
#pragma region DEPRECATED

	// Deprecated, old source/target
	UPROPERTY()
	FName SourceAttributeName_DEPRECATED = NAME_None;

	UPROPERTY()
	FName TargetAttributeName_DEPRECATED = NAME_None;

#pragma endregion

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExAttributeSourceToTargetDetails Attributes;

	/* If enabled, will auto-cast integer to double. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	bool bAutoCastIntegerToDouble = false;

	/** The default remap rule, used for single component values, or first component (X), or all components if no individual override is specified. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Remap (Default)"))
	FPCGExComponentRemapRule BaseRemap;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Individual Components", meta = (PCG_NotOverridable, InlineEditConditionToggle))
	bool bOverrideComponent2;

	/** Remap rule used for second (Y) value component. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Individual Components", meta = (PCG_NotOverridable, EditCondition="bOverrideComponent2", DisplayName="Remap (2nd Component)"))
	FPCGExComponentRemapRule Component2RemapOverride;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Individual Components", meta = (PCG_NotOverridable, InlineEditConditionToggle))
	bool bOverrideComponent3;

	/** Remap rule used for third (Z) value component. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Individual Components", meta = (PCG_NotOverridable, EditCondition="bOverrideComponent3", DisplayName="Remap (3rd Component)"))
	FPCGExComponentRemapRule Component3RemapOverride;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Individual Components", meta = (PCG_NotOverridable, InlineEditConditionToggle))
	bool bOverrideComponent4;

	/** Remap rule used for fourth (W) value component. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Individual Components", meta = (PCG_NotOverridable, EditCondition="bOverrideComponent4", DisplayName="Remap (4th Component)"))
	FPCGExComponentRemapRule Component4RemapOverride;

#if WITH_EDITOR
	FString GetDisplayName() const;
#endif

private:
	friend class FPCGExAttributeRemapElement;
};

struct FPCGExAttributeRemapContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExAttributeRemapElement;

	FPCGExComponentRemapRule RemapSettings[4];
	int32 RemapIndices[4];

	virtual void RegisterAssetDependencies() override;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExAttributeRemapElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(AttributeRemap)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual void PostLoadAssetsDependencies(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;

	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override;
};

namespace PCGExAttributeRemap
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExAttributeRemapContext, UPCGExAttributeRemapSettings>
	{
		EPCGMetadataTypes UnderlyingType = EPCGMetadataTypes::Unknown;
		int32 Dimensions = 0;

		TArray<TSharedPtr<PCGExData::TBufferProxy<double>>> InputProxies;
		TArray<TSharedPtr<PCGExData::TBufferProxy<double>>> OutputProxies;

		PCGExData::FProxyDescriptor InputDescriptor;
		PCGExData::FProxyDescriptor OutputDescriptor;

		TArray<FPCGExComponentRemapRule> Rules;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;

		void RemapRange(const PCGExMT::FScope& Scope);

		void OnPreparationComplete();

		virtual void CompleteWork() override;
	};
}
