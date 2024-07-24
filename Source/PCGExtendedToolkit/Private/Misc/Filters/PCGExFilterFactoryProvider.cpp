// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExFilterFactoryProvider.h"

#include "Data/PCGExPointFilter.h"

#define LOCTEXT_NAMESPACE "PCGExCreateFilter"
#define PCGEX_NAMESPACE PCGExCreateFilter

#if WITH_EDITOR
FString UPCGExFilterProviderSettings::GetDisplayName() const { return TEXT(""); }
#endif

UPCGExParamFactoryBase* UPCGExFilterProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	if (UPCGExFilterFactoryBase* OutFilterFactory = static_cast<UPCGExFilterFactoryBase*>(InFactory)) { OutFilterFactory->Priority = Priority; }
	return InFactory;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
