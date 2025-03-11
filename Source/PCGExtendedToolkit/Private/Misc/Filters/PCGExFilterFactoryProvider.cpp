// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExFilterFactoryProvider.h"

#include "Data/PCGExPointFilter.h"

#define LOCTEXT_NAMESPACE "PCGExCreateFilter"
#define PCGEX_NAMESPACE PCGExCreateFilter

#if WITH_EDITOR
FString UPCGExFilterProviderSettings::GetDisplayName() const { return TEXT(""); }
#endif

UPCGExFactoryData* UPCGExFilterProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExFilterFactoryData* NewFactory = Cast<UPCGExFilterFactoryData>(InFactory);
	NewFactory->Priority = Priority;

	return Super::CreateFactory(InContext, NewFactory);
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
