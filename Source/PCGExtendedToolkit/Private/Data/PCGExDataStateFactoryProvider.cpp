// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExDataStateFactoryProvider.h"

#include "PCGPin.h"
#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExCreateState"
#define PCGEX_NAMESPACE CreateState

TArray<FPCGPinProperties> UPCGExStateFactoryProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	FPCGPinProperties& ValidStatePin = PinProperties.Emplace_GetRef(PCGExDataState::SourceValidStateAttributesLabel, EPCGDataType::Param, true, true);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
	ValidStatePin.SetAdvancedPin();
#endif

#if WITH_EDITOR
	ValidStatePin.Tooltip = FTEXT("Attributes & values associated with this state when conditions are met.");
#endif

	FPCGPinProperties& InvalidStatePin = PinProperties.Emplace_GetRef(PCGExDataState::SourceInvalidStateAttributesLabel, EPCGDataType::Param, true, true);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
	InvalidStatePin.SetAdvancedPin();
#endif

#if WITH_EDITOR
	InvalidStatePin.Tooltip = FTEXT("Attributes & values associated with this state when conditions are not met.");
#endif

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExStateFactoryProviderSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(GetMainOutputLabel(), EPCGDataType::Param, false, false);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = FTEXT("Outputs a single state.");
#endif

	return PinProperties;
}

UPCGExParamFactoryBase* UPCGExStateFactoryProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	return Super::CreateFactory(InContext, InFactory);
}

bool UPCGExStateFactoryProviderSettings::ValidateStateName(const FPCGContext* Context) const
{
	if (!PCGEx::IsValidName(StateName))
	{
		PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("State name is invalid; Cannot be 'None' and can only contain the following special characters:[ ],[_],[-],[/]"));
		return false;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
