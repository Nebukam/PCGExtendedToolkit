// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Probes/PCGExProbeFactoryProvider.h"

#define LOCTEXT_NAMESPACE "PCGExCreateProbe"
#define PCGEX_NAMESPACE CreateProbe

UPCGExProbeOperation* UPCGExProbeFactoryBase::CreateOperation(FPCGExContext* InContext) const
{
	return nullptr; // Create probe operation
}

UPCGExParamFactoryBase* UPCGExProbeFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	return InFactory;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
