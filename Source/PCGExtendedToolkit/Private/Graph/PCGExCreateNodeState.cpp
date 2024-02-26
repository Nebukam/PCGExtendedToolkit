// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCreateNodeState.h"

#include "PCGPin.h"
#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExCreateNodeState"
#define PCGEX_NAMESPACE CreateNodeState

FPCGElementPtr UPCGExCreateNodeStateSettings::CreateElement() const { return MakeShared<FPCGExCreateNodeStateElement>(); }

TArray<FPCGPinProperties> UPCGExCreateNodeStateSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	FPCGPinProperties& InTestsPin = PinProperties.Emplace_GetRef(PCGExGraph::SourceTestsLabel, EPCGDataType::Param, true, true);

#if WITH_EDITOR
	InTestsPin.Tooltip = FTEXT("Tests performed to validate or invalidate this state.");
#endif

	FPCGPinProperties& IfPin = PinProperties.Emplace_GetRef(PCGExGraph::SourceIfAttributesLabel, EPCGDataType::Param, true, true);

#if WITH_EDITOR
	IfPin.Tooltip = FTEXT("Attributes & values associated with this state when conditions are met.");
#endif

	FPCGPinProperties& ElsePin = PinProperties.Emplace_GetRef(PCGExGraph::SourceElseAttributesLabel, EPCGDataType::Param, true, true);

#if WITH_EDITOR
	ElsePin.Tooltip = FTEXT("Attributes & values associated with this state when conditions are not met.");
#endif
	
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExCreateNodeStateSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& OutStatePin = PinProperties.Emplace_GetRef(PCGExGraph::OutputNodeStateLabel, EPCGDataType::Param, false, false);

#if WITH_EDITOR
	OutStatePin.Tooltip = FTEXT("Outputs a single node state.");
#endif

	return PinProperties;
}

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

	if (!PCGEx::IsValidName(Settings->StateName))
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("State name is invalid; Cannot be 'None' and can only contain the following special characters:[ ],[_],[-],[/]"));
		return true;
	}

	UPCGExNodeStateDefinition* OutState = NewObject<UPCGExNodeStateDefinition>();
	
	OutState->StateName = Settings->StateName;
	OutState->StateId = Settings->StateId;
	OutState->Priority = Settings->Priority;

	TArray<UPCGExAdjacencyTestDefinition*> TestDefinitions;
	
	const TArray<FPCGTaggedData>& TestPin = Context->InputData.GetInputsByPin(PCGExGraph::SourceTestsLabel);
	for (const FPCGTaggedData& TaggedData : TestPin)
	{
		if (const UPCGExAdjacencyTestDefinition* TestData = Cast<UPCGExAdjacencyTestDefinition>(TaggedData.Data))
		{
			TestDefinitions.Add(const_cast<UPCGExAdjacencyTestDefinition*>(TestData));
		}
	}

	if(TestDefinitions.IsEmpty())
	{
		OutState->ConditionalBeginDestroy();
		OutState->Tests.Append(TestDefinitions);
		PCGE_LOG(Error, GraphAndLog, FTEXT("No test data."));
		return true;
	}
	
	const TArray<FPCGTaggedData>& IfPin = Context->InputData.GetInputsByPin(PCGExGraph::SourceIfAttributesLabel);
	for (const FPCGTaggedData& TaggedData : IfPin)
	{
		if (const UPCGParamData* IfData = Cast<UPCGParamData>(TaggedData.Data))
		{
			OutState->IfAttributes.Add(const_cast<UPCGParamData*>(IfData));
			OutState->IfInfos.Add(PCGEx::FAttributesInfos::Get(IfData->Metadata));
		}
	}	

	const TArray<FPCGTaggedData>& ElsePin = Context->InputData.GetInputsByPin(PCGExGraph::SourceElseAttributesLabel);
	for (const FPCGTaggedData& TaggedData : ElsePin)
	{
		if (const UPCGParamData* ElseData = Cast<UPCGParamData>(TaggedData.Data))
		{
			OutState->ElseAttributes.Add(const_cast<UPCGParamData*>(ElseData));
			OutState->ElseInfos.Add(PCGEx::FAttributesInfos::Get(ElseData->Metadata));
		}
	}

	if (OutState->Tests.IsEmpty())
	{
		OutState->ConditionalBeginDestroy();
		return true;
	}

	FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
	Output.Data = OutState;
	Output.Pin = PCGExGraph::OutputSocketStateLabel;

	return true;
}

FPCGContext* FPCGExCreateNodeStateElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGContext* Context = new FPCGContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	return Context;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
