// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExSorting.h"

#include "PCGExCompare.h"
#include "PCGExGlobalSettings.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTag.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGExProxyData.h"

#define LOCTEXT_NAMESPACE "PCGExModularSortPoints"
#define PCGEX_NAMESPACE ModularSortPoints

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

PCG_DEFINE_TYPE_INFO(FPCGExDataTypeInfoSortRule, UPCGExSortingRule)

FPCGExSortRuleConfig::FPCGExSortRuleConfig(const FPCGExSortRuleConfig& Other)
	: FPCGExInputConfig(Other),
	  Tolerance(Other.Tolerance),
	  bInvertRule(Other.bInvertRule)
{
}

FPCGExCollectionSortingDetails::FPCGExCollectionSortingDetails(const bool InEnabled)
{
	bEnabled = InEnabled;
}

bool FPCGExCollectionSortingDetails::Init(const FPCGContext* InContext)
{
	if (!bEnabled) { return true; }
	return true;
}

void FPCGExCollectionSortingDetails::Sort(const FPCGExContext* InContext, const TSharedPtr<PCGExData::FPointIOCollection>& InCollection) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPointIOCollection::SortByTag);

	if (!bEnabled) { return; }

	const FString TagNameStr = TagName.ToString();
	TArray<double> Scores;

	TArray<TSharedPtr<PCGExData::FPointIO>>& Pairs = InCollection->Pairs;

	Scores.SetNumUninitialized(Pairs.Num());

#if WITH_EDITOR
	if (!bQuietMissingTagWarning)
	{
		for (int i = 0; i < Pairs.Num(); i++)
		{
			Pairs[i]->IOIndex = i;
			if (const TSharedPtr<PCGExData::IDataValue> Value = Pairs[i]->Tags->GetValue(TagNameStr))
			{
				Scores[i] = Value->GetValue<double>();
			}
			else
			{
				PCGEX_LOG_INVALID_INPUT(InContext, FText::Format(FTEXT("Some data is missing the '{0}' value tag."), FText::FromString(TagNameStr)))
				Scores[i] = (static_cast<double>(i) + FallbackOrderOffset) * FallbackOrderMultiplier;
			}
		}
	}
	else
#endif
	{
		for (int i = 0; i < Pairs.Num(); i++)
		{
			Pairs[i]->IOIndex = i;
			Scores[i] = Pairs[i]->Tags->GetValue(TagNameStr, (static_cast<double>(i) + FallbackOrderOffset) * FallbackOrderMultiplier);
		}
	}

	if (Direction == EPCGExSortDirection::Ascending)
	{
		Pairs.Sort([&](const TSharedPtr<PCGExData::FPointIO>& A, const TSharedPtr<PCGExData::FPointIO>& B) { return Scores[A->IOIndex] < Scores[B->IOIndex]; });
	}
	else
	{
		Pairs.Sort([&](const TSharedPtr<PCGExData::FPointIO>& A, const TSharedPtr<PCGExData::FPointIO>& B) { return Scores[A->IOIndex] > Scores[B->IOIndex]; });
	}

	for (int i = 0; i < Pairs.Num(); i++) { Pairs[i]->IOIndex = i; }
}

bool UPCGExSortingRule::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable;
	PCGEX_CONSUMABLE_SELECTOR(Config.Selector, Consumable)

	return true;
}

#if WITH_EDITOR
FLinearColor UPCGExSortingRuleProviderSettings::GetNodeTitleColor() const { return GetDefault<UPCGExGlobalSettings>()->ColorSortRule; }
#endif

UPCGExFactoryData* UPCGExSortingRuleProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExSortingRule* NewFactory = InContext->ManagedObjects->New<UPCGExSortingRule>();
	NewFactory->Priority = Priority;
	NewFactory->Config = Config;
	return Super::CreateFactory(InContext, NewFactory);
}

#if WITH_EDITOR
FString UPCGExSortingRuleProviderSettings::GetDisplayName() const { return Config.GetDisplayName(); }
#endif

namespace PCGExSorting
{
	void DeclareSortingRulesInputs(TArray<FPCGPinProperties>& PinProperties, const EPCGPinStatus InStatus)
	{
		FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(SourceSortingRules, EPCGDataType::Param);
		PCGEX_PIN_TOOLTIP("Plug sorting rules here. Order is defined by each rule' priority value, in ascending order.")
		Pin.PinStatus = InStatus;
	}

	FPointSorter::FPointSorter(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade, TArray<FPCGExSortRuleConfig> InRuleConfigs)
		: ExecutionContext(InContext), DataFacade(InDataFacade)
	{
		const UPCGData* InData = InDataFacade->Source->GetIn();
		FName Consumable = NAME_None;

		for (const FPCGExSortRuleConfig& RuleConfig : InRuleConfigs)
		{
			PCGEX_MAKE_SHARED(NewRule, FRuleHandler, RuleConfig)
			RuleHandlers.Add(NewRule);

			if (InContext->bCleanupConsumableAttributes && InData) { PCGEX_CONSUMABLE_SELECTOR(RuleConfig.Selector, Consumable) }
		}
	}

	FPointSorter::FPointSorter(TArray<FPCGExSortRuleConfig> InRuleConfigs)
	{
		for (const FPCGExSortRuleConfig& RuleConfig : InRuleConfigs)
		{
			PCGEX_MAKE_SHARED(NewRule, FRuleHandler, RuleConfig)
			RuleHandlers.Add(NewRule);
		}
	}

	bool FPointSorter::Init(FPCGExContext* InContext)
	{
		for (int i = 0; i < RuleHandlers.Num(); i++)
		{
			const TSharedPtr<FRuleHandler> RuleHandler = RuleHandlers[i];

			TSharedPtr<PCGExData::IBufferProxy> Buffer = nullptr;

			PCGExData::FProxyDescriptor Descriptor(DataFacade);
			Descriptor.bWantsDirect = true;

			if (Descriptor.CaptureStrict(InContext, RuleHandler->Selector, PCGExData::EIOSide::In)) { Buffer = PCGExData::GetProxyBuffer(InContext, Descriptor); }

			if (!Buffer)
			{
				RuleHandlers.RemoveAt(i);
				i--;
				PCGEX_LOG_INVALID_SELECTOR_C(InContext, Sorting Rule, RuleHandler->Selector)
				continue;
			}

			RuleHandler->Buffer = Buffer;
		}

		return !RuleHandlers.IsEmpty();
	}

	bool FPointSorter::Init(FPCGExContext* InContext, const TArray<TSharedRef<PCGExData::FFacade>>& InDataFacades)
	{
		int32 MaxIndex = 0;
		for (const TSharedRef<PCGExData::FFacade>& Facade : InDataFacades) { MaxIndex = FMath::Max(Facade->Idx, MaxIndex); }
		MaxIndex++;

		for (int i = 0; i < RuleHandlers.Num(); i++)
		{
			const TSharedPtr<FRuleHandler> RuleHandler = RuleHandlers[i];
			RuleHandler->Buffers.SetNum(MaxIndex);

			for (int f = 0; f < InDataFacades.Num(); f++)
			{
				TSharedPtr<PCGExData::FFacade> InFacade = InDataFacades[f];
				PCGExData::FProxyDescriptor Descriptor(InFacade);
				Descriptor.bWantsDirect = true;

				TSharedPtr<PCGExData::IBufferProxy> Buffer = nullptr;

				if (Descriptor.CaptureStrict(InContext, RuleHandler->Selector, PCGExData::EIOSide::In)) { Buffer = PCGExData::GetProxyBuffer(InContext, Descriptor); }

				if (!Buffer)
				{
					RuleHandlers.RemoveAt(i);
					i--;
					
					PCGEX_LOG_INVALID_SELECTOR_C(InContext, Sorting Rule, RuleHandler->Selector)
					break;
				}
				RuleHandler->Buffers[InFacade->Idx] = Buffer;
			}
		}

		return !RuleHandlers.IsEmpty();
	}

	bool FPointSorter::Init(FPCGExContext* InContext, const TArray<FPCGTaggedData>& InTaggedDatas)
	{
		IdxMap.Reserve(InTaggedDatas.Num());
		for (int i = 0; i < InTaggedDatas.Num(); i++) { IdxMap.Add(InTaggedDatas[i].Data->GetUniqueID(), i); }

		for (int i = 0; i < RuleHandlers.Num(); i++)
		{
			const TSharedPtr<FRuleHandler> RuleHandler = RuleHandlers[i];
			RuleHandler->DataValues.SetNum(InTaggedDatas.Num());

			for (int f = 0; f < InTaggedDatas.Num(); f++)
			{
				const UPCGData* Data = InTaggedDatas[f].Data;
				const int32 DataIdx = IdxMap[Data->GetUniqueID()];

				TSharedPtr<PCGExData::IDataValue> DataValue = PCGExData::TryGetValueFromData(Data, RuleHandler->Selector);

				if (!DataValue)
				{
					RuleHandlers.RemoveAt(i);
					i--;

					PCGEX_LOG_INVALID_SELECTOR_C(InContext, Sorting Rule, RuleHandler->Selector)
					break;
				}

				RuleHandler->DataValues[DataIdx] = DataValue;
			}
		}

		return !RuleHandlers.IsEmpty();
	}

	bool FPointSorter::Sort(const int32 A, const int32 B)
	{
		int Result = 0;
		for (const TSharedPtr<FRuleHandler>& RuleHandler : RuleHandlers)
		{
			const double ValueA = RuleHandler->Buffer->ReadAsDouble(A);
			const double ValueB = RuleHandler->Buffer->ReadAsDouble(B);
			Result = FMath::IsNearlyEqual(ValueA, ValueB, RuleHandler->Tolerance) ? 0 : ValueA < ValueB ? -1 : 1;
			if (Result != 0)
			{
				if (RuleHandler->bInvertRule) { Result *= -1; }
				break;
			}
		}

		if (SortDirection == EPCGExSortDirection::Descending) { Result *= -1; }
		return Result < 0;
	}

	bool FPointSorter::Sort(const PCGExData::FElement A, const PCGExData::FElement B)
	{
		int Result = 0;
		for (const TSharedPtr<FRuleHandler>& RuleHandler : RuleHandlers)
		{
			const double ValueA = RuleHandler->Buffers[A.IO]->ReadAsDouble(A.Index);
			const double ValueB = RuleHandler->Buffers[B.IO]->ReadAsDouble(B.Index);
			Result = FMath::IsNearlyEqual(ValueA, ValueB, RuleHandler->Tolerance) ? 0 : ValueA < ValueB ? -1 : 1;
			if (Result != 0)
			{
				if (RuleHandler->bInvertRule) { Result *= -1; }
				break;
			}
		}

		if (SortDirection == EPCGExSortDirection::Descending) { Result *= -1; }
		return Result < 0;
	}

	bool FPointSorter::SortData(const int32 A, const int32 B)
	{
		int Result = 0;
		for (const TSharedPtr<FRuleHandler>& RuleHandler : RuleHandlers)
		{
			const TSharedPtr<PCGExData::IDataValue> DataValueA = RuleHandler->DataValues[A];
			const TSharedPtr<PCGExData::IDataValue> DataValueB = RuleHandler->DataValues[B];

			if (!DataValueA || !DataValueB) { continue; }

			if (DataValueA->IsNumeric() || DataValueB->IsNumeric())
			{
				const double ValueA = DataValueA->AsDouble();
				const double ValueB = DataValueB->AsDouble();

				Result = FMath::IsNearlyEqual(ValueA, ValueB, RuleHandler->Tolerance) ? 0 : ValueA < ValueB ? -1 : 1;
			}
			else
			{
				const FString ValueA = DataValueA->AsString();
				const FString ValueB = DataValueB->AsString();

				Result = PCGExCompare::StrictlyEqual(ValueA, ValueB) ? 0 : ValueA < ValueB ? -1 : 1;
			}

			if (Result != 0)
			{
				if (RuleHandler->bInvertRule) { Result *= -1; }
				break;
			}
		}

		if (SortDirection == EPCGExSortDirection::Descending) { Result *= -1; }
		return Result < 0;
	}

	TArray<FPCGExSortRuleConfig> GetSortingRules(FPCGExContext* InContext, const FName InLabel)
	{
		TArray<FPCGExSortRuleConfig> OutRules;
		TArray<TObjectPtr<const UPCGExSortingRule>> Factories;
		if (!PCGExFactories::GetInputFactories(InContext, InLabel, Factories, {PCGExFactories::EType::RuleSort}, false)) { return OutRules; }
		for (const UPCGExSortingRule* Factory : Factories) { OutRules.Add(Factory->Config); }
		return OutRules;
	}
}
