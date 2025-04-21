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

TArray<FPCGExSortRuleConfig> PCGExSorting::GetSortingRules(FPCGExContext* InContext, const FName InLabel)
{
	TArray<FPCGExSortRuleConfig> OutRules;
	TArray<TObjectPtr<const UPCGExSortingRule>> Factories;
	if (!PCGExFactories::GetInputFactories(InContext, InLabel, Factories, {PCGExFactories::EType::RuleSort}, false)) { return OutRules; }
	for (const UPCGExSortingRule* Factory : Factories) { OutRules.Add(Factory->Config); }

	return OutRules;
}

void PCGExSorting::PrepareRulesAttributeBuffers(FPCGExContext* InContext, const FName InLabel, PCGExData::FFacadePreloader& FacadePreloader)
{
	TArray<TObjectPtr<const UPCGExSortingRule>> Factories;
	if (!PCGExFactories::GetInputFactories(InContext, InLabel, Factories, {PCGExFactories::EType::RuleSort}, false)) { return; }
	for (const UPCGExSortingRule* Factory : Factories) { FacadePreloader.Register<double>(InContext, Factory->Config.Selector); }
}

void PCGExSorting::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader, const TArray<FPCGExSortRuleConfig>& InRuleConfigs)
{
	for (const FPCGExSortRuleConfig& Rule : InRuleConfigs) { FacadePreloader.Register<double>(InContext, Rule.Selector); }
}
