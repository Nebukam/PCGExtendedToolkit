// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Sampling/PCGExSampling.h"


#include "PCGExAttributeStats.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExAttributeStatsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

	//~Begin UObject interface
#if WITH_EDITOR

public:
#endif
	//~End UObject interface

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(AttributeStats, "Attribute Stats", "Output attribute statistics.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMisc; }
#endif

protected:
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourceFiltersLabel, "Filters", PCGExFactories::PointFilters, false)
	//~End UPCGExPointsProcessorSettings

	/** Attributes to get. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExNameFiltersDetails Filters = FPCGExNameFiltersDetails(true);

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bOutputPerUniqueValuesStats = false;

	bool bOutputIdentifier = true;
	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, DisplayName = "Is Valid"))
	FName IdentifierAttributeName = FName(TEXT("Identifier"));

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOutputDefaultValue = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, DisplayName = "Default", EditCondition="bOutputDefaultValue"))
	FName DefaultValueAttributeName = FName(TEXT("Default"));

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOutputMinValue = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, DisplayName = "Min", EditCondition="bOutputMinValue"))
	FName MinValueAttributeName = FName(TEXT("Min"));

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOutputMaxValue = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, DisplayName = "Max", EditCondition="bOutputMaxValue"))
	FName MaxValueAttributeName = FName(TEXT("Max"));

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOutputSetMinValue = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, DisplayName = "Non-Default Min", EditCondition="bOutputSetMinValue"))
	FName SetMinValueAttributeName = FName(TEXT("SetMin"));

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOutputSetMaxValue = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, DisplayName = "Non-Default Max", EditCondition="bOutputSetMaxValue"))
	FName SetMaxValueAttributeName = FName(TEXT("SetMax"));

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOutputAverageValue = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, DisplayName = "Average", EditCondition="bOutputAverageValue"))
	FName AverageValueAttributeName = FName(TEXT("Average"));

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOutputUniqueValuesNum = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, DisplayName = "Unique Values Num", EditCondition="bOutputUniqueValuesNum"))
	FName UniqueValuesNumAttributeName = FName(TEXT("UniqueValues"));

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOutputUniqueSetValuesNum = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, DisplayName = "Unique Set Values Num", EditCondition="bOutputUniqueSetValuesNum"))
	FName UniqueSetValuesNumAttributeName = FName(TEXT("UniqueValues"));

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOutputDefaultValuesNum = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, DisplayName = "Default Values Num", EditCondition="bOutputDefaultValuesNum"))
	FName DefaultValuesNumAttributeName = FName(TEXT("DefaultValues"));

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOutputHasOnlyDefaultValues = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, DisplayName = "Has only Default Values", EditCondition="bOutputHasOnlyDefaultValues"))
	FName HasOnlyDefaultValuesAttributeName = FName(TEXT("bOnlyDefaults"));

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOutputHasOnlySetValues = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, DisplayName = "Has only Set Values", EditCondition="bOutputHasOnlySetValues"))
	FName HasOnlySetValuesAttributeName = FName(TEXT("bOnlyUniques"));

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOutputHasOnlyUniqueValues = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, DisplayName = "Has only Unique Values", EditCondition="bOutputHasOnlyUniqueValues"))
	FName HasOnlyUniqueValuesAttributeName = FName(TEXT("bOnlyUniques"));

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOutputSamples = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, DisplayName = "Has only Unique Values", EditCondition="bOutputSamples"))
	FName SamplesAttributeName = FName(TEXT("Samples"));

	bool bOutputIsValid = true;
	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, DisplayName = "Is Valid"))
	FName IsValidAttributeName = FName(TEXT("Valid"));


	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs (Unique Values)", meta = (PCG_Overridable, DisplayName = "Value Column", EditCondition="bOutputPerUniqueValuesStats"))
	FName UniqueValueAttributeName = FName(TEXT("Value"));

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs (Unique Values)", meta = (PCG_Overridable, EditCondition="bOutputPerUniqueValuesStats"))
	bool bOmitDefaultValue = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs (Unique Values)", meta = (PCG_Overridable, DisplayName = "Value Count", EditCondition="bOutputPerUniqueValuesStats"))
	FName ValueCountAttributeName = FName(TEXT("Count"));

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bQuietTypeMismatchWarning = false;

private:
	friend class FPCGExAttributeStatsElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAttributeStatsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExAttributeStatsElement;
	TSharedPtr<PCGEx::FAttributesInfos> AttributesInfos;

	TArray<UPCGParamData*> OutputParams;
	TMap<FName, UPCGParamData*> OutputParamsMap;
	TArray<int64> Rows;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAttributeStatsElement final : public FPCGExPointsProcessorElement
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExAttributeStats
{
	const FName OutputAttributeStats = FName("Stats");
	const FName OutputAttributeUniqueValues = FName("UniqueValues");

	class FAttributeStatsBase : public TSharedFromThis<FAttributeStatsBase>
	{
	public:
		const PCGEx::FAttributeIdentity Identity;
		const int64 Key;

		explicit FAttributeStatsBase(const PCGEx::FAttributeIdentity& InIdentity, const int64 InKey)
			: Identity(InIdentity), Key(InKey)
		{
		}

		virtual ~FAttributeStatsBase() = default;

		virtual void Process(
			const TSharedRef<PCGExData::FFacade> InDataFacade,
			FPCGExAttributeStatsContext* Context,
			const UPCGExAttributeStatsSettings* Settings,
			const TArray<bool>& Filter)
		{
		}
	};

	template <typename T>
	class TAttributeStats : public FAttributeStatsBase
	{
	public:
		T DefaultValue = T{};
		T MinValue = T{};
		T MaxValue = T{};
		T SetMinValue = T{};
		T SetMaxValue = T{};
		T AverageValue = T{};
		T AverageSetValue = T{};
		T MaxUniqueValue = T{};
		T MinUniqueValue = T{};
		int32 UniqueValuesNum = 0;
		int32 UniqueSetValuesNum = 0;
		int32 DefaultValuesNum = 0;

		explicit TAttributeStats(const PCGEx::FAttributeIdentity& InIdentity, const int64 InKey)
			: FAttributeStatsBase(InIdentity, InKey)
		{
		}

		virtual void Process(
			const TSharedRef<PCGExData::FFacade> InDataFacade,
			FPCGExAttributeStatsContext* Context,
			const UPCGExAttributeStatsSettings* Settings,
			const TArray<bool>& Filter) override
		{
			UPCGParamData* ParamData = Context->OutputParamsMap[Identity.Name];


#define PCGEX_OUTPUT_STAT(_NAME, _TYPE, _VALUE) if(Settings->bOutput##_NAME){ ParamData->Metadata->GetMutableTypedAttribute<_TYPE>(Settings->_NAME##AttributeName)->SetValue(Key, _VALUE); }

			TSharedPtr<PCGExData::TBuffer<T>> Buffer = InDataFacade->GetReadable<T>(Identity.Name);
			PCGExMath::TypeMinMax(MinValue, MaxValue);
			PCGExMath::TypeMinMax(SetMinValue, SetMaxValue);

			if (!Buffer)
			{
				// Invalid attribute, type mismatch!
				PCGEX_OUTPUT_STAT(IsValid, bool, false)
				return;
			}

			const FString Identifier = FString::Printf(TEXT("PCGEx/Identifier::%u"), InDataFacade->Source->GetIn()->GetUniqueID());
			PCGEX_OUTPUT_STAT(Identifier, FString, Identifier)

			if constexpr (!PCGEx::IsValidForTMap<T>::value)
			{
				// Unsupported types
				PCGEX_OUTPUT_STAT(IsValid, bool, false)
			}
			else
			{
				UPCGParamData* UniqueValuesParamData = nullptr;
				if (Settings->bOutputPerUniqueValuesStats)
				{
					UniqueValuesParamData = Context->ManagedObjects->New<UPCGParamData>();
					Context->StageOutput(OutputAttributeUniqueValues, UniqueValuesParamData, {Identifier, Identity.Name.ToString()}, false);
					InDataFacade->Source->Tags->Add(Identifier);
				}

				const int32 NumPoints = InDataFacade->GetNum();
				TMap<T, int32> ValuesCount;
				TMap<T, int32> SetValuesCount;
				ValuesCount.Reserve(NumPoints);
				SetValuesCount.Reserve(NumPoints);

				DefaultValue = Buffer->GetTypedInAttribute()->GetValueFromItemKey(PCGDefaultValueKey);
				int32 NumValues = 0;

				for (int i = 0; i < NumPoints; i++)
				{
					if (!Filter[i]) { continue; }
					NumValues++;

					const T& Value = Buffer->Read(i);

					MinValue = PCGExMath::Min(MinValue, Value);
					MaxValue = PCGExMath::Max(MaxValue, Value);

					AverageValue = PCGExMath::Add(AverageValue, Value);

					const int32 Count = ValuesCount.FindOrAdd(Value, 0);
					ValuesCount.Add(Value, Count + 1);

					if (PCGExCompare::StrictlyEqual(Value, DefaultValue))
					{
						DefaultValuesNum++;
					}
					else
					{
						const int32 SetCount = SetValuesCount.FindOrAdd(Value, 0);
						SetValuesCount.Add(Value, SetCount + 1);

						SetMinValue = PCGExMath::Min(SetMinValue, Value);
						SetMaxValue = PCGExMath::Max(SetMaxValue, Value);
					}
				}

				if (UniqueValuesParamData)
				{
					UPCGMetadata* UVM = UniqueValuesParamData->Metadata;
					FPCGMetadataAttribute<T>* UValues = UVM->FindOrCreateAttribute<T>(Settings->UniqueValueAttributeName, MinValue);
					FPCGMetadataAttribute<int32>* UCount = UVM->FindOrCreateAttribute<int32>(Settings->ValueCountAttributeName, 0);

					if (Settings->bOmitDefaultValue)
					{
						for (const TPair<T, int32>& Pair : SetValuesCount)
						{
							int64 UVKey = UVM->AddEntry();
							UValues->SetValue(UVKey, Pair.Key);
							UCount->SetValue(UVKey, Pair.Value);
						}
					}
					else
					{
						for (const TPair<T, int32>& Pair : ValuesCount)
						{
							int64 UVKey = UVM->AddEntry();
							UValues->SetValue(UVKey, Pair.Key);
							UCount->SetValue(UVKey, Pair.Value);
						}
					}
				}

				if (Settings->bOutputUniqueValuesNum)
				{
					UniqueValuesNum = 0;
					for (const TPair<T, int32>& Pair : ValuesCount) { if (Pair.Value == 1) { UniqueValuesNum++; } }
				}

				if (Settings->bOutputUniqueSetValuesNum)
				{
					UniqueSetValuesNum = 0;
					for (const TPair<T, int32>& Pair : SetValuesCount) { if (Pair.Value == 1) { UniqueSetValuesNum++; } }
				}


				ValuesCount.Empty();
				SetValuesCount.Empty();

				////// OUTPUT

				PCGEX_OUTPUT_STAT(DefaultValue, T, DefaultValue)
				PCGEX_OUTPUT_STAT(MinValue, T, MinValue)
				PCGEX_OUTPUT_STAT(MaxValue, T, MaxValue)
				PCGEX_OUTPUT_STAT(SetMinValue, T, SetMinValue)
				PCGEX_OUTPUT_STAT(SetMaxValue, T, SetMaxValue)
				PCGEX_OUTPUT_STAT(AverageValue, T, PCGExMath::Div(AverageValue, static_cast<double>(NumValues)))
				PCGEX_OUTPUT_STAT(UniqueValuesNum, int32, UniqueValuesNum)
				PCGEX_OUTPUT_STAT(UniqueSetValuesNum, int32, UniqueSetValuesNum)
				PCGEX_OUTPUT_STAT(DefaultValuesNum, int32, DefaultValuesNum)
				PCGEX_OUTPUT_STAT(HasOnlyDefaultValues, bool, NumValues == DefaultValuesNum)
				PCGEX_OUTPUT_STAT(HasOnlySetValues, bool, DefaultValuesNum == 0)
				PCGEX_OUTPUT_STAT(HasOnlyUniqueValues, bool, NumValues == UniqueSetValuesNum)
				PCGEX_OUTPUT_STAT(Samples, int32, NumValues)
				PCGEX_OUTPUT_STAT(IsValid, bool, true)

#undef PCGEX_OUTPUT_STAT
			}
		}
	};

	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExAttributeStatsContext, UPCGExAttributeStatsSettings>
	{
		TArray<TSharedPtr<FAttributeStatsBase>> Stats;
		TMap<FName, int32> PerAttributeStatMap;
		TArray<UPCGParamData*> PerAttributeStats;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool IsTrivial() const override { return false; }
		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void CompleteWork() override;
	};
}
