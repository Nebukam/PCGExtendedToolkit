// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExSplineInclusionFilter.h"

#define LOCTEXT_NAMESPACE "PCGExSplineInclusionFilterDefinition"
#define PCGEX_NAMESPACE PCGExSplineInclusionFilterDefinition

TArray<FPCGPinProperties> UDEPRECATED_PCGExSplineInclusionFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POLYLINES(FName("Splines"), TEXT("Splines will be used for testing"), Required)
	return PinProperties;
}

UPCGExFactoryData* UDEPRECATED_PCGExSplineInclusionFilterProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("This filter is deprecated, use 'Filter : Inclusion' instead."));
	return nullptr;
}

#if WITH_EDITOR
FString UDEPRECATED_PCGExSplineInclusionFilterProviderSettings::GetDisplayName() const
{
	return PCGExPathInclusion::ToString(Config.CheckType);
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
