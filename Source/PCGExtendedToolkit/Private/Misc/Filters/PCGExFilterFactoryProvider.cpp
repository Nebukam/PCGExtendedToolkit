// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExFilterFactoryProvider.h"

#include "PCGExHelpers.h"
#include "PCGExLabels.h"
#include "Data/PCGExPointFilter.h"
#include "Misc/Filters/PCGExConstantFilter.h"

#define LOCTEXT_NAMESPACE "PCGExCreateFilter"
#define PCGEX_NAMESPACE PCGExCreateFilter

#if WITH_EDITOR
FString UPCGExFilterProviderSettings::GetDisplayName() const { return TEXT(""); }
#endif

UPCGExFilterProviderSettings::UPCGExFilterProviderSettings()
{
	Priority = GetDefaultPriority();
}

FName UPCGExFilterProviderSettings::GetMainOutputPin() const { return PCGExPointFilter::OutputFilterLabel; }

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

FName UPCGExFilterCollectionProviderSettings::GetMainOutputPin() const{ return PCGExPointFilter::OutputColFilterLabel; }

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
