// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sorting/PCGExSortingDetails.h"

#include "Data/PCGExDataTags.h"
#include "Data/PCGExDataValue.h"
#include "Data/PCGExPointIO.h"
#include "Sorting/PCGExSortingRuleProvider.h"

#define LOCTEXT_NAMESPACE "PCGExModularSortPoints"
#define PCGEX_NAMESPACE ModularSortPoints

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

FPCGExSortRuleConfig::FPCGExSortRuleConfig(const FPCGExSortRuleConfig& Other)
	: FPCGExInputConfig(Other), Tolerance(Other.Tolerance), bInvertRule(Other.bInvertRule)
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

namespace PCGExSorting
{
	void DeclareSortingRulesInputs(TArray<FPCGPinProperties>& PinProperties, const EPCGPinStatus InStatus)
	{
		FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(Labels::SourceSortingRules, FPCGExDataTypeInfoSortRule::AsId());
		PCGEX_PIN_TOOLTIP("Plug sorting rules here. Order is defined by each rule' priority value, in ascending order.")
		Pin.PinStatus = InStatus;
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
