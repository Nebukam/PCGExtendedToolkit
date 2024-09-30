// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Misc/PCGExModularSortPoints.h"

#define LOCTEXT_NAMESPACE "PCGExModularSortPoints"
#define PCGEX_NAMESPACE ModularSortPoints

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

UPCGExParamFactoryBase* UPCGExSortingRuleProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExSortingRule* NewFactory = NewObject<UPCGExSortingRule>();
	NewFactory->Priority = Priority;
	NewFactory->Config = Config;
	return NewFactory;
}

#if WITH_EDITOR
FString UPCGExSortingRuleProviderSettings::GetDisplayName() const { return Config.GetDisplayName(); }
#endif

TArray<FPCGPinProperties> UPCGExModularSortPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(TEXT("SortRules"), "Plug sorting rules here. Order is defined by each rule' priority value, in ascending order.", Required, {})
	return PinProperties;
}

bool UPCGExModularSortPointsSettings::GetSortingRules(const FPCGContext* InContext, TArray<FPCGExSortRuleConfig>& OutRules) const
{
	TArray<TObjectPtr<const UPCGExSortingRule>> Factories;
	if (!PCGExFactories::GetInputFactories(
		InContext, TEXT("SortRules"), Factories,
		{PCGExFactories::EType::RuleSort}, false))
	{
		return false;
	}

	Factories.Sort([](const UPCGExSortingRule& A, const UPCGExSortingRule& B) { return A.Priority < B.Priority; });
	for (const UPCGExSortingRule* Factory : Factories) { OutRules.Add(Factory->Config); }

	return true;
}
