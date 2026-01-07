// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExProbeFactoryProvider.h"

#include "Clusters/PCGExClusterCommon.h"

#define LOCTEXT_NAMESPACE "PCGExCreateProbe"
#define PCGEX_NAMESPACE CreateProbe

PCG_DEFINE_TYPE_INFO(FPCGExDataTypeInfoProbe, UPCGExProbeFactoryData)

TSharedPtr<FPCGExProbeOperation> UPCGExProbeFactoryData::CreateOperation(FPCGExContext* InContext) const
{
	return nullptr; // Create probe operation
}

FName UPCGExProbeFactoryProviderSettings::GetMainOutputPin() const { return PCGExClusters::Labels::OutputProbeLabel; }

UPCGExFactoryData* UPCGExProbeFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	return Super::CreateFactory(InContext, InFactory);
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
