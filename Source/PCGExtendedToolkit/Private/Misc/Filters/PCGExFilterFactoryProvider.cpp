// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExFilterFactoryProvider.h"

#include "Data/PCGExPointFilter.h"
#include "Misc/Filters/PCGExConstantFilter.h"

#define LOCTEXT_NAMESPACE "PCGExCreateFilter"
#define PCGEX_NAMESPACE PCGExCreateFilter

#if WITH_EDITOR
FString UPCGExFilterProviderSettings::GetDisplayName() const { return TEXT(""); }
#endif

UPCGExFactoryData* UPCGExFilterProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExFilterFactoryData* NewFactory = Cast<UPCGExFilterFactoryData>(InFactory);
	NewFactory->MissingDataHandling = MissingDataHandling;
	NewFactory->Priority = Priority;

	return Super::CreateFactory(InContext, NewFactory);
}

bool UPCGExFilterProviderSettings::ShouldCancel(FPCGExFactoryProviderContext* InContext, PCGExFactories::EPreparationResult InResult) const
{
	if (MissingDataHandling == EPCGExFilterNoDataFallback::Error) { return Super::ShouldCancel(InContext, InResult); }

	UPCGExConstantFilterFactory* NewFactory = InContext->ManagedObjects->New<UPCGExConstantFilterFactory>();

	NewFactory->Priority = Priority;
	NewFactory->Config.bInvert = false;
	NewFactory->Config.Value = MissingDataHandling == EPCGExFilterNoDataFallback::Pass;
	
	InContext->OutFactory = NewFactory;
	
	return false;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
