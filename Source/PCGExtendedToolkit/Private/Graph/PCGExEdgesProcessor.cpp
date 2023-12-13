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
	FPCGPinProperties& PinEdgesInput = PinProperties.Emplace_GetRef(PCGExGraph::SourceEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinEdgesInput.Tooltip = FTEXT("Edges associated with the main input points");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExEdgesProcessorSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	FPCGPinProperties& PinEdgesOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinEdgesOutput.Tooltip = FTEXT("Edges associated with the main output points");
#endif // WITH_EDITOR

	return PinProperties;
}

#pragma endregion

PCGEX_INITIALIZE_CONTEXT(EdgesProcessor)

bool FPCGExEdgesProcessorElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(EdgesProcessor)

	return true;
}

FPCGContext* FPCGExEdgesProcessorElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	FPCGExPointsProcessorElementBase::InitializeContext(InContext, InputData, SourceComponent, Node);

	PCGEX_CONTEXT(EdgesProcessor)

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExGraph::SourceParamsLabel);

	return Context;
}


#undef LOCTEXT_NAMESPACE
