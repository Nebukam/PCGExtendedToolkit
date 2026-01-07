// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/Meta/NeighborSamplers/PCGExNeighborSampleAttribute.h"

#define LOCTEXT_NAMESPACE "PCGExCreateNeighborSample"
#define PCGEX_NAMESPACE PCGExCreateNeighborSample

#if WITH_EDITOR
FString UPCGExNeighborSampleAttributeSettings::GetDisplayName() const { return TEXT("DEPRECATED"); }
#endif

TSharedPtr<FPCGExNeighborSampleOperation> UPCGExNeighborSamplerFactoryAttribute::CreateOperation(FPCGExContext* InContext) const
{
	return nullptr;
}

UPCGExFactoryData* UPCGExNeighborSampleAttributeSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("SampleAttribute is deprecated, use `Sample Blend` with blend ops instead."));
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
