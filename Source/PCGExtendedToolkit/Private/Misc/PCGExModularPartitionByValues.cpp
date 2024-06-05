// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExModularPartitionByValues.h"

#define LOCTEXT_NAMESPACE "PCGExModularPartitionByValues"
#define PCGEX_NAMESPACE ModularPartitionByValues

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

PCGExFactories::EType UPCGExPartitionRule::GetFactoryType() const { return PCGExFactories::EType::PartitionRule; }

FName UPCGExPartitionRuleProviderSettings::GetMainOutputLabel() const { return FName(TEXT("PartitionRule")); }

UPCGExParamFactoryBase* UPCGExPartitionRuleProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExPartitionRule* NewFactory = NewObject<UPCGExPartitionRule>();
	NewFactory->Descriptor = Descriptor;
	return NewFactory;
}

FString UPCGExPartitionRuleProviderSettings::GetDisplayName() const
{
	return Descriptor.GetDisplayName();
}

TArray<FPCGPinProperties> UPCGExModularPartitionByValuesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(TEXT("PartitionRules"), "Plug partitions rules here.", Required, {})
	return PinProperties;
}

bool UPCGExModularPartitionByValuesSettings::GetPartitionRules(const FPCGContext* InContext, TArray<FPCGExPartitonRuleDescriptor>& OutRules) const
{
	TArray<UPCGExPartitionRule*> Factories;
	if (!PCGExFactories::GetInputFactories(
		InContext, TEXT("PartitionRules"),
		Factories, {PCGExFactories::EType::PartitionRule}, false))
	{
		return false;
	}

	for (const UPCGExPartitionRule* Factory : Factories) { OutRules.Add(Factory->Descriptor); }

	return true;
}
