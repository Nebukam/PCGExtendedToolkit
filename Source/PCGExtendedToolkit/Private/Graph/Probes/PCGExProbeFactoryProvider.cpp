// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Probes/PCGExProbeFactoryProvider.h"

#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExCreateProbe"
#define PCGEX_NAMESPACE CreateProbe

TSharedPtr<FPCGExProbeOperation> UPCGExProbeFactoryData::CreateOperation(FPCGExContext* InContext) const
{
	return nullptr; // Create probe operation
}

FName UPCGExProbeFactoryProviderSettings::GetMainOutputPin() const { return PCGExGraph::OutputProbeLabel; }

UPCGExFactoryData* UPCGExProbeFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	return Super::CreateFactory(InContext, InFactory);
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
