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
	FPCGPinProperties& InTestsPin = PinProperties.EmplaceAt_GetRef(0, PCGExDataState::SourceTestsLabel, EPCGDataType::Param, true, true);

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

	TArray<UPCGExAdjacencyTestDefinition*> TestDefinitions;

	const TArray<FPCGTaggedData>& TestPin = Context->InputData.GetInputsByPin(PCGExDataState::SourceTestsLabel);
	for (const FPCGTaggedData& TaggedData : TestPin)
	{
		if (const UPCGExAdjacencyTestDefinition* TestData = Cast<UPCGExAdjacencyTestDefinition>(TaggedData.Data))
		{
			TestDefinitions.Add(const_cast<UPCGExAdjacencyTestDefinition*>(TestData));
		}
	}

	if (TestDefinitions.IsEmpty())
	{
		OutState->ConditionalBeginDestroy();
		OutState->Tests.Append(TestDefinitions);
		PCGE_LOG(Error, GraphAndLog, FTEXT("No test data."));
		return true;
	}

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
