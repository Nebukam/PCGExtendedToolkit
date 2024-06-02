// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCreateNodeState.h"

#include "PCGPin.h"
#include "Graph/PCGExCluster.h"
#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExCreateNodeState"
#define PCGEX_NAMESPACE CreateNodeState

FName UPCGExCreateNodeStateSettings::GetMainOutputLabel() const { return PCGExCluster::OutputNodeStateLabel; }

TArray<FPCGPinProperties> UPCGExCreateNodeStateSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	FPCGPinProperties& InTestsPin = PinProperties.EmplaceAt_GetRef(0, PCGExDataState::SourceFiltersLabel, EPCGDataType::Param, true, true);

#if WITH_EDITOR
	InTestsPin.Tooltip = FTEXT("Tests performed to validate or invalidate this state.");
#endif
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
	InTestsPin.SetRequiredPin();
#endif

	return PinProperties;
}

UPCGExParamFactoryBase* UPCGExCreateNodeStateSettings::CreateFactory(FPCGContext* Context, UPCGExParamFactoryBase* InFactory) const
{
	if (!ValidateStateName(Context)) { return nullptr; }

	TArray<UPCGExFilterFactoryBase*> FilterFactories;
	if (!PCGExDataFilter::GetInputFactories(Context, PCGExDataState::SourceFiltersLabel, FilterFactories, PCGExFactories::ClusterFilters)) { return nullptr; }

	UPCGExNodeStateFactory* OutState = CreateStateFactory<UPCGExNodeStateFactory>(Context);
	OutState->FilterFactories.Append(FilterFactories);

	return OutState;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
