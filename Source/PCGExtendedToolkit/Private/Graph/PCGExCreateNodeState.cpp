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


FPCGElementPtr UPCGExCreateNodeStateSettings::CreateElement() const { return MakeShared<FPCGExCreateNodeStateElement>(); }

#if WITH_EDITOR
void UPCGExCreateNodeStateSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

bool FPCGExCreateNodeStateElement::ExecuteInternal(
	FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCreateNodeStateElement::Execute);

	PCGEX_SETTINGS(CreateNodeState)

	if (!Boot(Context)) { return true; }

	UPCGExNodeStateDefinition* OutState = CreateStateDefinition<UPCGExNodeStateDefinition>(Context);

	TArray<UPCGExFilterDefinitionBase*> FilterDefinitions;

	const TArray<FPCGTaggedData>& TestPin = Context->InputData.GetInputsByPin(PCGExDataState::SourceFiltersLabel);
	for (const FPCGTaggedData& TaggedData : TestPin)
	{
		if (const UPCGExFilterDefinitionBase* TestData = Cast<UPCGExFilterDefinitionBase>(TaggedData.Data))
		{
			FilterDefinitions.Add(const_cast<UPCGExFilterDefinitionBase*>(TestData));
		}
	}

	if (FilterDefinitions.IsEmpty())
	{
		OutState->ConditionalBeginDestroy();
		PCGE_LOG(Error, GraphAndLog, FTEXT("No test data."));
		return true;
	}
	OutState->Tests.Append(FilterDefinitions);

	if (OutState->Tests.IsEmpty())
	{
		OutState->ConditionalBeginDestroy();
		return true;
	}

	OutputState(Context, OutState);

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
