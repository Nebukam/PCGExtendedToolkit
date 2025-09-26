// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExSplineAlphaFilter.h"

#include "Data/PCGPolyLineData.h"

#define LOCTEXT_NAMESPACE "PCGExSplineAlphaFilterDefinition"
#define PCGEX_NAMESPACE PCGExSplineAlphaFilterDefinition

TArray<FPCGPinProperties> UDEPRECATED_PCGExSplineAlphaFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POLYLINES(FName("Splines"), TEXT("Splines will be used for testing"), Required)
	return PinProperties;
}

UPCGExFactoryData* UDEPRECATED_PCGExSplineAlphaFilterProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("This filter is deprecated, use 'Filter : Time' instead."));
	return nullptr;
}

#if WITH_EDITOR
FString UDEPRECATED_PCGExSplineAlphaFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = TEXT("Alpha ") + PCGExCompare::ToString(Config.Comparison);

	if (Config.CompareAgainst == EPCGExInputValueType::Attribute) { DisplayName += PCGEx::GetSelectorDisplayName(Config.OperandB); }
	else { DisplayName += FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.OperandBConstant) / 1000.0)); }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
