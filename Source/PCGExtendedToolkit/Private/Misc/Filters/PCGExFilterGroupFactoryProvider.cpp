// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Misc/Filters/PCGExFilterGroupFactoryProvider.h"

#include "Data/PCGExPointFilter.h"
#include "Data/PCGExFilterGroup.h"


#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExCreateFilterGroup"
#define PCGEX_NAMESPACE PCGExCreateFilterGroup

#if WITH_EDITOR
FString UPCGExFilterGroupProviderSettings::GetDisplayName() const { return Mode == EPCGExFilterGroupMode::OR ? TEXT("OR") : TEXT("AND"); }


TArray<FPCGPreConfiguredSettingsInfo> UPCGExFilterGroupProviderSettings::GetPreconfiguredInfo() const
{
	TArray<FPCGPreConfiguredSettingsInfo> Infos;
	Infos.Emplace_GetRef(0, FTEXT("PCGEx | Filter AND"));
	Infos.Emplace_GetRef(1, FTEXT("PCGEx | Filter OR"));
	return Infos;
}
#endif

void UPCGExFilterGroupProviderSettings::ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo)
{
	Super::ApplyPreconfiguredSettings(PreconfigureInfo);
	Mode = PreconfigureInfo.PreconfiguredIndex == 0 ? EPCGExFilterGroupMode::AND : EPCGExFilterGroupMode::OR;
}

TArray<FPCGPinProperties> UPCGExFilterGroupProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_FACTORIES(PCGExPointFilter::SourceFiltersLabel, "List of filters that will be processed in either AND or OR mode.", Required, {})
	return PinProperties;
}

UPCGExFactoryData* UPCGExFilterGroupProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExFilterGroupFactoryData* NewFactory;

	if (Mode == EPCGExFilterGroupMode::AND) { NewFactory = InContext->ManagedObjects->New<UPCGExFilterGroupFactoryDataAND>(); }
	else { NewFactory = InContext->ManagedObjects->New<UPCGExFilterGroupFactoryDataOR>(); }

	NewFactory->Priority = Priority;
	NewFactory->bInvert = bInvert;

	if (!GetInputFactories(
		InContext, PCGExPointFilter::SourceFiltersLabel, NewFactory->FilterFactories,
		PCGExFactories::AnyFilters, true))
	{
		InContext->ManagedObjects->Destroy(NewFactory);
		return nullptr;
	}

	return NewFactory;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
