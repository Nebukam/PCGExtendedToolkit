// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGEx.h"
#include "PCGExGlobalSettings.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExAttributeHelpers.h"


#include "PCGExAttributeRemap.generated.h"


USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExClampDetails
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bApplyClampMin = false;

	/** Clamp minimum value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bApplyClampMin"))
	double ClampMinValue = 0;

	/** Clamp maximum value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
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
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExRemapDetails
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bUseAbsoluteRange = true;

	/** Whether or not to preserve value sign when using absolute range.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bUseAbsoluteRange"))
	bool bPreserveSign = true;

	/** Fixed In Min value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bUseInMin = false;

	/** Fixed In Min value. If disabled, will use the lowest input value.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bUseInMin"))
	double InMin = 0;

	/** Fixed In Max value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bUseInMax = false;

	/** Fixed In Max value. If disabled, will use the highest input value.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bUseInMax"))
	double InMax = 0;

	/** How to remap before sampling the curve. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExTruncateMode TruncateOutput = EPCGExTruncateMode::None;

	/** Scale the value after it's been truncated. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="TruncateOutput != EPCGExTruncateMode::None", EditConditionHides))
	double PostTruncateScale = 1;

	void Init()
	{
		if (!bUseLocalCurve) { LocalScoreCurve.ExternalCurve = RemapCurve.Get(); }
		RemapCurveObj = LocalScoreCurve.GetRichCurveConst();
	}

	FORCEINLINE double GetRemappedValue(const double Value) const
	{
		return PCGEx::TruncateDbl(
			RemapCurveObj->Eval(PCGExMath::Remap(Value, InMin, InMax, 0, 1)) * Scale,
			TruncateOutput);
	}
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExComponentRemapRule
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExClampDetails InputClampDetails;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExRemapDetails RemapDetails;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExClampDetails OutputClampDetails;

	TSharedPtr<PCGExMT::TScopedValue<double>> MinCache;
	TSharedPtr<PCGExMT::TScopedValue<double>> MaxCache;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExAttributeRemapSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(AttributeRemap, "Attribute Remap", "Remap a single property or attribute.", FName(GetDisplayName()));
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->NodeColorMiscWrite); }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual void PostLoad() override;

	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

#if WITH_EDITORONLY_DATA
	// Deprecated, old source/target

	UPROPERTY()
	FName SourceAttributeName_DEPRECATED = NAME_None;

	UPROPERTY()
	FName TargetAttributeName_DEPRECATED = NAME_None;
#endif

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExAttributeSourceToTargetDetails Attributes;

	/** The default remap rule, used for single component values, or first component. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Remap (Default)"))
	FPCGExComponentRemapRule BaseRemap;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NoteOverridable, InlineEditConditionToggle))
	bool bOverrideComponent2;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NoteOverridable, EditCondition="bOverrideComponent2", DisplayName="Remap (2nd Component)"))
	FPCGExComponentRemapRule Component2RemapOverride;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NoteOverridable, InlineEditConditionToggle))
	bool bOverrideComponent3;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NoteOverridable, EditCondition="bOverrideComponent3", DisplayName="Remap (3rd Component)"))
	FPCGExComponentRemapRule Component3RemapOverride;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NoteOverridable, InlineEditConditionToggle))
	bool bOverrideComponent4;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NoteOverridable, EditCondition="bOverrideComponent4", DisplayName="Remap (4th Component)"))
	FPCGExComponentRemapRule Component4RemapOverride;

#if WITH_EDITOR
	FString GetDisplayName() const;
#endif

	
private:
	friend class FPCGExAttributeRemapElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAttributeRemapContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExAttributeRemapElement;

	FPCGExComponentRemapRule RemapSettings[4];
	int32 RemapIndices[4];

	virtual void RegisterAssetDependencies() override;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAttributeRemapElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual void PostLoadAssetsDependencies(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExAttributeRemap
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExAttributeRemapContext, UPCGExAttributeRemapSettings>
	{
		EPCGMetadataTypes UnderlyingType = EPCGMetadataTypes::Unknown;
		int32 Dimensions = 0;

		TArray<FPCGExComponentRemapRule> Rules;

		TSharedPtr<PCGExData::FBufferBase> CacheWriter = nullptr;
		TSharedPtr<PCGExData::FBufferBase> CacheReader = nullptr;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;

		template <typename T>
		void RemapRange(const PCGExMT::FScope& Scope, T DummyValue)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExAttributeRemap::RemapRange);

			PCGExData::TBuffer<T>* Writer = static_cast<PCGExData::TBuffer<T>*>(CacheWriter.Get());

			for (int d = 0; d < Dimensions; d++)
			{
				FPCGExComponentRemapRule& Rule = Rules[d];

				double VAL;

				if (Rule.RemapDetails.bUseAbsoluteRange)
				{
					if (Rule.RemapDetails.bPreserveSign)
					{
						for (int i = Scope.Start; i < Scope.End; i++)
						{
							T& V = Writer->GetMutable(i);
							VAL = PCGExMath::GetComponent(V, d);
							VAL = Rule.RemapDetails.GetRemappedValue(FMath::Abs(VAL)) * PCGExMath::SignPlus(VAL);
							VAL = Rule.OutputClampDetails.GetClampedValue(VAL);

							PCGExMath::SetComponent(V, d, VAL);
						}
					}
					else
					{
						for (int i = Scope.Start; i < Scope.End; i++)
						{
							T& V = Writer->GetMutable(i);
							VAL = PCGExMath::GetComponent(V, d);
							VAL = Rule.RemapDetails.GetRemappedValue(FMath::Abs(VAL));
							VAL = Rule.OutputClampDetails.GetClampedValue(VAL);

							PCGExMath::SetComponent(V, d, VAL);
						}
					}
				}
				else
				{
					if (Rule.RemapDetails.bPreserveSign)
					{
						for (int i = Scope.Start; i < Scope.End; i++)
						{
							T& V = Writer->GetMutable(i);
							VAL = PCGExMath::GetComponent(V, d);
							VAL = Rule.RemapDetails.GetRemappedValue(VAL);
							VAL = Rule.OutputClampDetails.GetClampedValue(VAL);

							PCGExMath::SetComponent(V, d, VAL);
						}
					}
					else
					{
						for (int i = Scope.Start; i < Scope.End; i++)
						{
							T& V = Writer->GetMutable(i);
							VAL = PCGExMath::GetComponent(V, d);
							VAL = Rule.RemapDetails.GetRemappedValue(FMath::Abs(VAL));
							VAL = Rule.OutputClampDetails.GetClampedValue(VAL);

							PCGExMath::SetComponent(V, d, VAL);
						}
					}
				}
			}
		}

		void OnPreparationComplete();

		virtual void CompleteWork() override;
	};
}
