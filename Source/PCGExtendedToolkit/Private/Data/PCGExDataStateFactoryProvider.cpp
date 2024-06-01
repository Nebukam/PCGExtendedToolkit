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
	PCGEX_PIN_PARAMS(PCGExDataState::SourceValidStateAttributesLabel, "Attributes & values associated with this state when conditions are met.", true, {})
	PCGEX_PIN_PARAMS(PCGExDataState::SourceInvalidStateAttributesLabel, "Attributes & values associated with this state when conditions are not met.", true, {})
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExStateFactoryProviderSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_PARAM(GetMainOutputLabel(), "Outputs a single state.", false, {})
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
