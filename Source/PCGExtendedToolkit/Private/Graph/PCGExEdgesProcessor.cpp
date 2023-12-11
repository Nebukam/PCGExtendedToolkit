// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExEdgesProcessor.h"

#include "IPCGExDebug.h"
#include "Data/PCGExGraphParamsData.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface


TArray<FPCGPinProperties> UPCGExEdgesProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	FPCGPinProperties& PinPropertyParams = PinProperties.Emplace_GetRef(PCGExGraph::SourceParamsLabel, EPCGDataType::Param);

#if WITH_EDITOR
	PinPropertyParams.Tooltip = LOCTEXT("PCGExSourceParamsPinTooltip", "Graph Params. Data is de-duped internally.");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExEdgesProcessorSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	FPCGPinProperties& PinParamsOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputParamsLabel, EPCGDataType::Param);

#if WITH_EDITOR
	PinParamsOutput.Tooltip = LOCTEXT("PCGExOutputParamsTooltip", "Graph Params forwarding. Data is de-duped internally.");
#endif // WITH_EDITOR

	return PinProperties;
}

#pragma endregion

FPCGContext* FPCGExEdgesProcessorElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExEdgesProcessorContext* Context = new FPCGExEdgesProcessorContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	return Context;
}

bool FPCGExEdgesProcessorElement::Validate(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Validate(InContext)) { return false; }

	PCGEX_CONTEXT(FPCGExEdgesProcessorContext)

	if (Context->Graphs.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingParams", "Missing Input Params."));
		return false;
	}

	return true;
}

void FPCGExEdgesProcessorElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	FPCGExPointsProcessorElementBase::InitializeContext(InContext, InputData, SourceComponent, Node);

	FPCGExEdgesProcessorContext* Context = static_cast<FPCGExEdgesProcessorContext*>(InContext);

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExGraph::SourceParamsLabel);
	Context->Graphs.Initialize(InContext, Sources);
}


#undef LOCTEXT_NAMESPACE
