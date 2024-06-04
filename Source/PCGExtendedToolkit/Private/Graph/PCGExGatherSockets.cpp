// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExGatherSockets.h"

#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExGatherSockets"
#define PCGEX_NAMESPACE GatherSockets

UPCGExGatherSocketsSettings::UPCGExGatherSocketsSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RefreshSocketNames();
}

void UPCGExGatherSocketsSettings::RefreshSocketNames()
{
	GeneratedSocketNames.Empty();
	for (const FPCGExSocketDescriptor& Socket : InputSockets)
	{
		FPCGExSocketQualityOfLifeInfos& Infos = GeneratedSocketNames.Emplace_GetRef();
		Infos.Populate(GraphIdentifier, Socket);
	}
}

const TArray<FPCGExSocketDescriptor>& UPCGExGatherSocketsSettings::GetSockets() const { return InputSockets; }

FPCGElementPtr UPCGExGatherSocketsSettings::CreateElement() const { return MakeShared<FPCGExGatherSocketsElement>(); }

TArray<FPCGPinProperties> UPCGExGatherSocketsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_PARAMS(PCGExGraph::SourceSocketParamsLabel, "Socket params to assemble into a consolidated Custom Graph Params object.", Required, {})
	PCGEX_PIN_PARAM(PCGExGraph::SourceSocketOverrideParamsLabel, "Socket params used as a reference for global overriding.", Advanced, {})
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExGatherSocketsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_PARAM(PCGExGraph::SourceSingleGraphLabel, "Outputs a unified graph param object.", Required, {})
	return PinProperties;
}

#if WITH_EDITOR
void UPCGExGatherSocketsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	RefreshSocketNames();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

bool FPCGExGatherSocketsElement::ExecuteInternal(
	FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExGatherSocketsElement::Execute);

	UPCGExGatherSocketsSettings* Settings = const_cast<UPCGExGatherSocketsSettings*>(Context->GetInputSettings<UPCGExGatherSocketsSettings>());

	if (Settings->GraphIdentifier.IsNone() || !FPCGMetadataAttributeBase::IsValidName(Settings->GraphIdentifier.ToString()))
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Graph Identifier is invalid; Cannot be 'None' and can only contain the following special characters:[ ],[_],[-],[/]"));
		return true;
	}

	const TArray<FPCGTaggedData>& Inputs = Context->InputData.TaggedData;
	Settings->InputSockets.Empty();

	TArray<FPCGExSocketDescriptor> InputSockets;
	TArray<FPCGExSocketDescriptor> OmittedSockets;

	PCGExGraph::GetUniqueSocketParams(Context, PCGExGraph::SourceSocketParamsLabel, InputSockets, OmittedSockets);

	for (const FPCGExSocketDescriptor& SocketDescriptor : InputSockets)
	{
		if (SocketDescriptor.SocketName.IsNone()) { continue; }
		Settings->InputSockets.Add(SocketDescriptor);
	}

	if (!OmittedSockets.IsEmpty())
	{
		for (const FPCGExSocketDescriptor& IgnoredSocket : OmittedSockets)
		{
			PCGE_LOG(Warning, GraphAndLog, FText::Format(FTEXT("Socket name {0} already exists."), FText::FromName(IgnoredSocket.SocketName)));
		}
	}

	if (Settings->InputSockets.IsEmpty())
	{
		Settings->RefreshSocketNames();
		PCGE_LOG(Error, GraphAndLog, FTEXT("Found no socket data to assemble."));
		return true;
	}

	Settings->RefreshSocketNames();

	PCGExGraph::GetUniqueSocketParams(Context, PCGExGraph::SourceSocketOverrideParamsLabel, InputSockets, OmittedSockets);

	FPCGExSocketGlobalOverrides Overrides = Settings->GlobalOverrides;
	Overrides.bEnabled = Settings->bApplyGlobalOverrides;

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	UPCGExGraphDefinition* OutParams = PCGExGraph::FGraphInputs::NewGraph(
		Context->Node->GetUniqueID(),
		Settings->GraphIdentifier,
		Settings->GetSockets(),
		Overrides,
		InputSockets.IsEmpty() ? FPCGExSocketDescriptor(NAME_None) : InputSockets[0]);

	FPCGTaggedData& Output = Outputs.Emplace_GetRef();
	Output.Data = OutParams;
	Output.Pin = PCGExGraph::OutputForwardGraphsLabel;

	return true;
}

FPCGContext* FPCGExGatherSocketsElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGContext* Context = new FPCGContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	return Context;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
