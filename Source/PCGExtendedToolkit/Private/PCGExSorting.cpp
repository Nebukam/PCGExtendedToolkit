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
	return NewFactory;
}

#if WITH_EDITOR
FString UPCGExSortingRuleProviderSettings::GetDisplayName() const { return Config.GetDisplayName(); }
#endif
