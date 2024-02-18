// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCreateCustomGraphSocket.h"

#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExCreateCustomGraphSocket"
#define PCGEX_NAMESPACE CreateCustomGraphSocket

FPCGElementPtr UPCGExCreateCustomGraphSocketSettings::CreateElement() const { return MakeShared<FPCGExCreateCustomGraphSocketElement>(); }

TArray<FPCGPinProperties> UPCGExCreateCustomGraphSocketSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> NoInput;
	return NoInput;
}

TArray<FPCGPinProperties> UPCGExCreateCustomGraphSocketSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputSocketParamsLabel, EPCGDataType::Param, false, false);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = FTEXT("Outputs a single socket that needs to be assembled.");
#endif

	return PinProperties;
}

#if WITH_EDITOR
void UPCGExCreateCustomGraphSocketSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

bool FPCGExCreateCustomGraphSocketElement::ExecuteInternal(
	FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCreateCustomGraphSocketElement::Execute);

	PCGEX_SETTINGS(CreateCustomGraphSocket)

	if (Settings->Socket.SocketName.IsNone() || !FPCGMetadataAttributeBase::IsValidName(Settings->Socket.SocketName.ToString()))
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Output name is invalid; Cannot be 'None' and can only contain the following special characters:[ ],[_],[-],[/]"));
		return false;
	}

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	UPCGExSocketDefinition* OutParams = NewObject<UPCGExSocketDefinition>();
	OutParams->Descriptor = FPCGExSocketDescriptor(Settings->Socket);

	FPCGTaggedData& Output = Outputs.Emplace_GetRef();
	Output.Data = OutParams;
	Output.Pin = PCGExGraph::OutputSocketParamsLabel;

	return true;
}

FPCGContext* FPCGExCreateCustomGraphSocketElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGContext* Context = new FPCGContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	return Context;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
