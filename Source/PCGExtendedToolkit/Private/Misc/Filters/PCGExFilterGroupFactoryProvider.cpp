// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Misc/Filters/PCGExFilterGroupFactoryProvider.h"

#include "Data/PCGExPointFilter.h"
#include "Data/PCGExFilterGroup.h"


#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExCreateFilterGroup"
#define PCGEX_NAMESPACE PCGExCreateFilterGroup

#if WITH_EDITOR
FString UPCGExFilterGroupProviderSettings::GetDisplayName() const { return Mode == EPCGExFilterGroupMode::OR ? TEXT("OR") : TEXT("AND"); }
#endif

TArray<FPCGPinProperties> UPCGExFilterGroupProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_PARAMS(PCGExPointFilter::SourceFiltersLabel, "List of filters that will be processed in either AND or OR mode.", Required, {})
	return PinProperties;
}

UPCGExParamFactoryBase* UPCGExFilterGroupProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExFilterGroupFactoryBase* NewFactory;

	if (Mode == EPCGExFilterGroupMode::AND) { NewFactory = InContext->ManagedObjects->New<UPCGExFilterGroupFactoryBaseAND>(); }
	else { NewFactory = InContext->ManagedObjects->New<UPCGExFilterGroupFactoryBaseOR>(); }

	NewFactory->Priority = Priority;
	NewFactory->bInvert = bInvert;

	if (!GetInputFactories(
		InContext, PCGExPointFilter::SourceFiltersLabel, NewFactory->FilterFactories,
		PCGExFactories::AnyFilters, true))
	{
		InContext->ManagedObjects->Destroy(NewFactory);
	}

	return NewFactory;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
