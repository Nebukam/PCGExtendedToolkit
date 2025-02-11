// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "PCGExSorting.h"

#define LOCTEXT_NAMESPACE "PCGExModularSortPoints"
#define PCGEX_NAMESPACE ModularSortPoints

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

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
