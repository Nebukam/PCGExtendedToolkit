// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExPolygonInclusionFilter.h"
#include "Paths/PCGExPaths.h"


#define LOCTEXT_NAMESPACE "PCGExPolygonInclusionFilterDefinition"
#define PCGEX_NAMESPACE PCGExPolygonInclusionFilterDefinition

TArray<FPCGPinProperties> UDEPRECATED_PCGExPolygonInclusionFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_ANY(PCGExPaths::SourcePathsLabel, TEXT("Paths or splines that will be used for testing"), Required)
	return PinProperties;
}

UPCGExFactoryData* UDEPRECATED_PCGExPolygonInclusionFilterProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("This filter is deprecated, use 'Filter : Inclusion' instead."));
	return nullptr;
}

#if WITH_EDITOR
FString UDEPRECATED_PCGExPolygonInclusionFilterProviderSettings::GetDisplayName() const
{
	return TEXT("Inside Polygon");
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
