// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCustomGraphProcessor.h"

#include "Data/PCGExGraphDefinition.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface


TArray<FPCGPinProperties> UPCGExCustomGraphProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExGraph::SourceSingleGraphLabel, "Graph Params. Data is de-duped internally.", Required, {})
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExCustomGraphProcessorSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_PARAMS(PCGExGraph::OutputForwardGraphsLabel, "Graph Params forwarding. Data is de-duped internally.", Required, {})
	return PinProperties;
}

FName UPCGExCustomGraphProcessorSettings::GetMainInputLabel() const { return PCGExGraph::SourceGraphsLabel; }
FName UPCGExCustomGraphProcessorSettings::GetMainOutputLabel() const { return PCGExGraph::OutputGraphsLabel; }

FPCGExCustomGraphProcessorContext::~FPCGExCustomGraphProcessorContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(CachedIndexReader)
	PCGEX_DELETE(CachedIndexWriter)

	SocketInfos.Empty();

	if (CurrentGraph) { CurrentGraph->Cleanup(); }
}

#pragma endregion

bool FPCGExCustomGraphProcessorContext::AdvanceGraph(const bool bResetPointsIndex)
{
	if (bResetPointsIndex) { CurrentPointIOIndex = -1; }

	if (CurrentGraph) { CurrentGraph->Cleanup(); }

	if (Graphs.Params.IsValidIndex(++CurrentParamsIndex))
	{
		CurrentGraph = Graphs.Params[CurrentParamsIndex];
		CurrentGraph->GetSocketsInfos(SocketInfos);
		CurrentGraphEdgeCrawlingTypes = EdgeCrawlingSettings.GetCrawlingEdgeTypes(CurrentGraph->GraphIdentifier);
		return true;
	}

	CurrentGraph = nullptr;
	return false;
}

bool FPCGExCustomGraphProcessorContext::AdvancePointsIOAndResetGraph()
{
	CurrentParamsIndex = -1;
	return AdvancePointsIO();
}

void FPCGExCustomGraphProcessorContext::Reset()
{
	FPCGExPointsProcessorContext::Reset();
	CurrentParamsIndex = -1;
}

void FPCGExCustomGraphProcessorContext::SetCachedIndex(const int32 PointIndex, const int32 Index) const
{
	check(!bReadOnly)
	(*CachedIndexWriter)[PointIndex] = Index;
}

int32 FPCGExCustomGraphProcessorContext::GetCachedIndex(const int32 PointIndex) const
{
	if (bReadOnly) { return (*CachedIndexReader)[PointIndex]; }
	return (*CachedIndexWriter)[PointIndex];
}

void FPCGExCustomGraphProcessorContext::WriteSocketInfos() const
{
	CachedIndexWriter->Write();
	for (const PCGExGraph::FSocketInfos& Infos : SocketInfos) { Infos.Socket->Write(); }
}

bool FPCGExCustomGraphProcessorContext::PrepareCurrentGraphForPoints(const PCGExData::FPointIO& PointIO, const bool ReadOnly)
{
	bValidCurrentGraph = false;
	bReadOnly = ReadOnly;

	if (bReadOnly)
	{
		PCGEX_DELETE(CachedIndexWriter)
		if (!CachedIndexReader) { CachedIndexReader = new PCGEx::TFAttributeReader<int32>(CurrentGraph->CachedIndexAttributeName); }
		bValidCurrentGraph = CachedIndexReader->Bind(const_cast<PCGExData::FPointIO&>(PointIO));
	}
	else
	{
		PCGEX_DELETE(CachedIndexReader)
		if (!CachedIndexWriter) { CachedIndexWriter = new PCGEx::TFAttributeWriter<int32>(CurrentGraph->CachedIndexAttributeName, -1, false); }
		bValidCurrentGraph = CachedIndexWriter->BindAndGet(const_cast<PCGExData::FPointIO&>(PointIO));
	}

	if (bValidCurrentGraph) { CurrentGraph->PrepareForPointData(PointIO, bReadOnly); }
	return bValidCurrentGraph;
}

PCGEX_INITIALIZE_CONTEXT(CustomGraphProcessor)

void FPCGExCustomGraphProcessorElement::DisabledPassThroughData(FPCGContext* Context) const
{
	FPCGExPointsProcessorElementBase::DisabledPassThroughData(Context);

	//Forward edges
	TArray<FPCGTaggedData> GraphsSources = Context->InputData.GetInputsByPin(PCGExGraph::SourceSingleGraphLabel);
	for (const FPCGTaggedData& TaggedData : GraphsSources)
	{
		FPCGTaggedData& TaggedDataCopy = Context->OutputData.TaggedData.Emplace_GetRef();
		TaggedDataCopy.Data = TaggedData.Data;
		TaggedDataCopy.Tags.Append(TaggedData.Tags);
		TaggedDataCopy.Pin = PCGExGraph::OutputForwardGraphsLabel;
	}
}

bool FPCGExCustomGraphProcessorElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT(CustomGraphProcessor)

	if (Context->Graphs.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing Input Params."));
		return false;
	}

	Context->MergedInputSocketsNum = 0;
	for (const UPCGExGraphDefinition* Graph : Context->Graphs.Params) { Context->MergedInputSocketsNum += Graph->GetSocketMapping()->NumSockets; }

	return true;
}

FPCGContext* FPCGExCustomGraphProcessorElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	FPCGExPointsProcessorElementBase::InitializeContext(InContext, InputData, SourceComponent, Node);

	PCGEX_CONTEXT_AND_SETTINGS(CustomGraphProcessor)

	if (!Settings->bEnabled) { return Context; }

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExGraph::SourceSingleGraphLabel);
	Context->Graphs.Initialize(InContext, Sources);

	return Context;
}


#undef LOCTEXT_NAMESPACE
