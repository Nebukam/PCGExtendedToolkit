// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/Meta/NeighborSamplers/PCGExNeighborSampleProperties.h"

#define LOCTEXT_NAMESPACE "PCGExCreateNeighborSample"
#define PCGEX_NAMESPACE PCGExCreateNeighborSample

#if WITH_EDITOR
FString UPCGExNeighborSamplePropertiesSettings::GetDisplayName() const { return TEXT("DEPRECATED"); }
#endif

TSharedPtr<FPCGExNeighborSampleOperation> UPCGExNeighborSamplerFactoryProperties::CreateOperation(FPCGExContext* InContext) const
{
	return nullptr;
}

UPCGExFactoryData* UPCGExNeighborSamplePropertiesSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("`Sampler : Vtx Properties` is deprecated, use `Sampler : Vtx Blend` with blend ops instead."));
	return nullptr;
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
