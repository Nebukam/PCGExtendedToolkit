// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCreateNodeTest.h"

#include "PCGPin.h"
#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExCreateNodeTest"
#define PCGEX_NAMESPACE CreateNodeTest

FPCGElementPtr UPCGExCreateNodeTestSettings::CreateElement() const { return MakeShared<FPCGExCreateNodeTestElement>(); }

TArray<FPCGPinProperties> UPCGExCreateNodeTestSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExCreateNodeTestSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputTestLabel, EPCGDataType::Param, false, false);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = FTEXT("Outputs a single test definition to be used by a node state.");
#endif

	return PinProperties;
}

#if WITH_EDITOR
void UPCGExCreateNodeTestSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Descriptor.GetDisplayName();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

bool FPCGExCreateNodeTestElement::ExecuteInternal(
	FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCreateNodeTestElement::Execute);

	PCGEX_SETTINGS(CreateNodeTest)

	UPCGExAdjacencyTestDefinition* OutTest = NewObject<UPCGExAdjacencyTestDefinition>();
	OutTest->Descriptor = Settings->Descriptor;

	FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
	Output.Data = OutTest;
	Output.Pin = PCGExGraph::OutputSocketStateLabel;

	return true;
}

FPCGContext* FPCGExCreateNodeTestElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGContext* Context = new FPCGContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	return Context;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
