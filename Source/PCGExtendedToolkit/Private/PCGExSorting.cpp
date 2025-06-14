// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExSorting.h"

#define LOCTEXT_NAMESPACE "PCGExModularSortPoints"
#define PCGEX_NAMESPACE ModularSortPoints

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

bool FPCGExCollectionSortingDetails::Init(const FPCGContext* InContext)
{
	if (!bEnabled) { return true; }
	return true;
}

void FPCGExCollectionSortingDetails::Sort(const FPCGContext* InContext, const TSharedPtr<PCGExData::FPointIOCollection>& InCollection) const
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
			if (const TSharedPtr<PCGExTags::FTagValue> Value = Pairs[i]->Tags->GetValue(TagNameStr))
			{
				Scores[i] = Value->GetValue<double>();
			}
			else
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Some data is missing the '{0}' value tag."), FText::FromString(TagNameStr)));
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
	FPointSorter::FPointSorter(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade, TArray<FPCGExSortRuleConfig> InRuleConfigs)
		: ExecutionContext(InContext), DataFacade(InDataFacade)
	{
		const UPCGData* InData = InDataFacade->Source->GetIn();
		FName Consumable = NAME_None;

		for (const FPCGExSortRuleConfig& RuleConfig : InRuleConfigs)
		{
			PCGEX_MAKE_SHARED(NewRule, FPCGExSortRule, RuleConfig)
			Rules.Add(NewRule);

			if (InContext->bCleanupConsumableAttributes && InData) { PCGEX_CONSUMABLE_SELECTOR(RuleConfig.Selector, Consumable) }
		}
	}

	FPointSorter::FPointSorter(TArray<FPCGExSortRuleConfig> InRuleConfigs)
	{
		for (const FPCGExSortRuleConfig& RuleConfig : InRuleConfigs)
		{
			PCGEX_MAKE_SHARED(NewRule, FPCGExSortRule, RuleConfig)
			Rules.Add(NewRule);
		}
	}

	bool FPointSorter::Init(FPCGExContext* InContext)
	{
		for (int i = 0; i < Rules.Num(); i++)
		{
			const TSharedPtr<FPCGExSortRule> Rule = Rules[i];

			TSharedPtr<PCGExData::IBufferProxy> Buffer = nullptr;

			PCGExData::FProxyDescriptor Descriptor(DataFacade);
			Descriptor.bWantsDirect = true;

			if (Descriptor.CaptureStrict(InContext, Rule->Selector, PCGExData::EIOSide::In)) { Buffer = PCGExData::GetProxyBuffer(InContext, Descriptor); }

			if (!Buffer)
			{
				Rules.RemoveAt(i);
				i--;

				PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Some points are missing attributes used for sorting."));
				continue;
			}

			Rule->Buffer = Buffer;
		}

		return !Rules.IsEmpty();
	}

	bool FPointSorter::Init(FPCGExContext* InContext, const TArray<TSharedRef<PCGExData::FFacade>>& InDataFacades)
	{
		int32 MaxIndex = 0;
		for (const TSharedRef<PCGExData::FFacade>& Facade : InDataFacades) { MaxIndex = FMath::Max(Facade->Idx, MaxIndex); }
		MaxIndex++;

		for (int i = 0; i < Rules.Num(); i++)
		{
			const TSharedPtr<FPCGExSortRule> Rule = Rules[i];
			Rule->Buffers.SetNum(MaxIndex);

			for (int f = 0; f < InDataFacades.Num(); f++)
			{
				TSharedPtr<PCGExData::FFacade> InFacade = InDataFacades[f];
				PCGExData::FProxyDescriptor Descriptor(InFacade);
				Descriptor.bWantsDirect = true;

				TSharedPtr<PCGExData::IBufferProxy> Buffer = nullptr;

				if (Descriptor.CaptureStrict(InContext, Rule->Selector, PCGExData::EIOSide::In)) { Buffer = PCGExData::GetProxyBuffer(InContext, Descriptor); }

				if (!Buffer)
				{
					Rules.RemoveAt(i);
					i--;

					PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Some points are missing attributes used for sorting."));
					break;
				}
				Rule->Buffers[InFacade->Idx] = Buffer;
			}
		}

		return !Rules.IsEmpty();
	}

	bool FPointSorter::Sort(const int32 A, const int32 B)
	{
		int Result = 0;
		for (const TSharedPtr<FPCGExSortRule>& Rule : Rules)
		{
			const double ValueA = Rule->Buffer->ReadAsDouble(A);
			const double ValueB = Rule->Buffer->ReadAsDouble(B);
			Result = FMath::IsNearlyEqual(ValueA, ValueB, Rule->Tolerance) ? 0 : ValueA < ValueB ? -1 : 1;
			if (Result != 0)
			{
				if (Rule->bInvertRule) { Result *= -1; }
				break;
			}
		}

		if (SortDirection == EPCGExSortDirection::Descending) { Result *= -1; }
		return Result < 0;
	}

	bool FPointSorter::Sort(const PCGExData::FElement A, const PCGExData::FElement B)
	{
		int Result = 0;
		for (const TSharedPtr<FPCGExSortRule>& Rule : Rules)
		{
			const double ValueA = Rule->Buffers[A.IO]->ReadAsDouble(A.Index);
			const double ValueB = Rule->Buffers[B.IO]->ReadAsDouble(B.Index);
			Result = FMath::IsNearlyEqual(ValueA, ValueB, Rule->Tolerance) ? 0 : ValueA < ValueB ? -1 : 1;
			if (Result != 0)
			{
				if (Rule->bInvertRule) { Result *= -1; }
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
