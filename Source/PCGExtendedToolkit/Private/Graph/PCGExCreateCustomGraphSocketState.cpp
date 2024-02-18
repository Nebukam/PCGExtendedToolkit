// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCreateCustomGraphSocketState.h"

#include "PCGPin.h"
#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExCreateCustomGraphSocketState"
#define PCGEX_NAMESPACE CreateCustomGraphSocketState

FPCGElementPtr UPCGExCreateCustomGraphSocketStateSettings::CreateElement() const { return MakeShared<FPCGExCreateCustomGraphSocketStateElement>(); }

TArray<FPCGPinProperties> UPCGExCreateCustomGraphSocketStateSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

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

TArray<FPCGPinProperties> UPCGExCreateCustomGraphSocketStateSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputSocketStateLabel, EPCGDataType::Param, false, false);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = FTEXT("Outputs a single socket state.");
#endif

	return PinProperties;
}

#if WITH_EDITOR
void UPCGExCreateCustomGraphSocketStateSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

bool FPCGExCreateCustomGraphSocketStateElement::ExecuteInternal(
	FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCreateCustomGraphSocketStateElement::Execute);

	PCGEX_SETTINGS(CreateCustomGraphSocketState)

	if (!PCGEx::IsValidName(Settings->StateName))
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("State name is invalid; Cannot be 'None' and can only contain the following special characters:[ ],[_],[-],[/]"));
		return true;;
	}

	UPCGExSocketStateDefinition* OutState = NewObject<UPCGExSocketStateDefinition>();

	const TArray<FPCGTaggedData>& IfPin = Context->InputData.GetInputsByPin(PCGExGraph::SourceIfAttributesLabel);
	for (const FPCGTaggedData& TaggedData : IfPin)
	{
		if (UPCGParamData* IfData = Cast<UPCGParamData>(TaggedData.Data)) { OutState->IfAttributes.Add(IfData); }
	}

	OutState->StateName = Settings->StateName;
	OutState->StateId = Settings->StateId;

	const TArray<FPCGTaggedData>& ElsePin = Context->InputData.GetInputsByPin(PCGExGraph::SourceElseAttributesLabel);
	for (const FPCGTaggedData& TaggedData : ElsePin)
	{
		if (UPCGParamData* ElseData = Cast<UPCGParamData>(TaggedData.Data)) { OutState->ElseAttributes.Add(ElseData); }
	}

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	for (const FPCGExSocketConditionDescriptor& Descriptor : Settings->Conditions)
	{
		if (!Descriptor.bEnabled) { continue; }

		if (Descriptor.SocketName.IsNone() || !FPCGMetadataAttributeBase::IsValidName(Descriptor.SocketName.ToString()))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("A socket name is invalid; Cannot be 'None' and can only contain the following special characters:[ ],[_],[-],[/]"));
			continue;
		}

		OutState->Conditions.Add(Descriptor);
	}

	if(OutState->Conditions.IsEmpty())
	{
		OutState->ConditionalBeginDestroy();
		return true;
	}

	FPCGTaggedData& Output = Outputs.Emplace_GetRef();
	Output.Data = OutState;
	Output.Pin = PCGExGraph::OutputSocketStateLabel;

	return true;
}

FPCGContext* FPCGExCreateCustomGraphSocketStateElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGContext* Context = new FPCGContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	return Context;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
