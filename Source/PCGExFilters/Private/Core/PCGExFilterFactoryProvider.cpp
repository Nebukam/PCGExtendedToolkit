// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExFilterFactoryProvider.h"

#include "Containers/PCGExManagedObjects.h"
#include "PCGExFilterCommon.h"
#include "Filters/Points/PCGExConstantFilter.h"

#define LOCTEXT_NAMESPACE "PCGExCreateFilter"
#define PCGEX_NAMESPACE PCGExCreateFilter

#if WITH_EDITOR
FString UPCGExFilterProviderSettings::GetDisplayName() const { return TEXT(""); }
#endif

UPCGExFilterProviderSettings::UPCGExFilterProviderSettings()
{
	Priority = GetDefaultPriority();
}

FName UPCGExFilterProviderSettings::GetMainOutputPin() const { return PCGExFilters::Labels::OutputFilterLabel; }

UPCGExFactoryData* UPCGExFilterProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExPointFilterFactoryData* NewFactory = Cast<UPCGExPointFilterFactoryData>(InFactory);
	NewFactory->MissingDataPolicy = MissingDataPolicy;
	NewFactory->Priority = Priority;

	return Super::CreateFactory(InContext, NewFactory);
}

bool UPCGExFilterProviderSettings::ShouldCancel(FPCGExFactoryProviderContext* InContext, PCGExFactories::EPreparationResult InResult) const
{
	if (MissingDataPolicy == EPCGExFilterNoDataFallback::Error) { return Super::ShouldCancel(InContext, InResult); }

	UPCGExConstantFilterFactory* NewFactory = InContext->ManagedObjects->New<UPCGExConstantFilterFactory>();

	NewFactory->Priority = Priority;
	NewFactory->Config.bInvert = false;
	NewFactory->Config.Value = MissingDataPolicy == EPCGExFilterNoDataFallback::Pass;

	if (InContext->OutFactory) { InContext->ManagedObjects->Destroy(InContext->OutFactory); }
	InContext->OutFactory = NewFactory;

	return false;
}

FName UPCGExFilterCollectionProviderSettings::GetMainOutputPin() const { return PCGExFilters::Labels::OutputColFilterLabel; }

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
