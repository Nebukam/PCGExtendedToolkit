// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExModularPartitionByValues.h"

#define LOCTEXT_NAMESPACE "PCGExModularPartitionByValues"
#define PCGEX_NAMESPACE ModularPartitionByValues

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

UPCGExParamFactoryBase* UPCGExPartitionRuleProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExPartitionRule* NewFactory = InContext->ManagedObjects->New<UPCGExPartitionRule>();
	NewFactory->Config = Config;
	return NewFactory;
}

#if WITH_EDITOR
FString UPCGExPartitionRuleProviderSettings::GetDisplayName() const { return Config.GetDisplayName(); }
#endif

TArray<FPCGPinProperties> UPCGExModularPartitionByValuesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(TEXT("PartitionRules"), "Plug partitions rules here.", Required, {})
	return PinProperties;
}

bool UPCGExModularPartitionByValuesSettings::GetPartitionRules(const FPCGContext* InContext, TArray<FPCGExPartitonRuleConfig>& OutRules) const
{
	TArray<TObjectPtr<const UPCGExPartitionRule>> Factories;
	if (!PCGExFactories::GetInputFactories(
		InContext, TEXT("PartitionRules"), Factories,
		{PCGExFactories::EType::RulePartition}, false))
	{
		return false;
	}

	for (const UPCGExPartitionRule* Factory : Factories) { OutRules.Add(Factory->Config); }

	return true;
}
