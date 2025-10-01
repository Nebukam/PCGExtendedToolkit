// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExPathAlphaFilter.h"

#include "PCGExHelpers.h"
#include "Details/PCGExDetailsSettings.h"
#include "Paths/PCGExPaths.h"

#define LOCTEXT_NAMESPACE "PCGExPathAlphaFilterDefinition"
#define PCGEX_NAMESPACE PCGExPathAlphaFilterDefinition

PCGEX_SETTING_VALUE_GET_IMPL(FPCGExPathAlphaFilterConfig, OperandB, double, CompareAgainst, OperandB, OperandBConstant)

TArray<FPCGPinProperties> UDEPRECATED_PCGExPathAlphaFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINTS(PCGExPaths::SourcePathsLabel, TEXT("Paths will be used for testing"), Required)
	return PinProperties;
}

UPCGExFactoryData* UDEPRECATED_PCGExPathAlphaFilterProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("This filter is deprecated, use 'Filter : Time' instead."));
	return nullptr;
}

#if WITH_EDITOR
FString UDEPRECATED_PCGExPathAlphaFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = TEXT("Alpha ") + PCGExCompare::ToString(Config.Comparison);

	if (Config.CompareAgainst == EPCGExInputValueType::Attribute) { DisplayName += PCGEx::GetSelectorDisplayName(Config.OperandB); }
	else { DisplayName += FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.OperandBConstant) / 1000.0)); }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
