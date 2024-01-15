// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExEdgesProcessor.h"

#include "IPCGExDebug.h"
#include "Data/PCGExGraphParamsData.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface


PCGExData::EInit UPCGExEdgesProcessorSettings::GetMainOutputInitMode() const { return PCGExData::EInit::Forward; }

FName UPCGExEdgesProcessorSettings::GetMainInputLabel() const { return PCGExGraph::SourceVerticesLabel; }
FName UPCGExEdgesProcessorSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGExData::EInit UPCGExEdgesProcessorSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::Forward; }

bool UPCGExEdgesProcessorSettings::GetMainAcceptMultipleData() const { return true; }

TArray<FPCGPinProperties> UPCGExEdgesProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	FPCGPinProperties& PinEdgesInput = PinProperties.Emplace_GetRef(PCGExGraph::SourceEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinEdgesInput.Tooltip = FTEXT("Edges associated with the main input points");
#endif

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExEdgesProcessorSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	FPCGPinProperties& PinEdgesOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinEdgesOutput.Tooltip = FTEXT("Edges associated with the main output points");
#endif

	return PinProperties;
}

#pragma endregion

FPCGExEdgesProcessorContext::~FPCGExEdgesProcessorContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(InputDictionary)
	PCGEX_DELETE(MainEdges)
	PCGEX_DELETE(CurrentCluster)

	NodeIndicesMap.Empty();
}


bool FPCGExEdgesProcessorContext::AdvancePointsIO()
{
	PCGEX_DELETE(EdgeNumReader)
	CurrentEdgesIndex = -1;
	NodeIndicesMap.Empty();

	if (!FPCGExPointsProcessorContext::AdvancePointsIO()) { return false; }

	if (FString CurrentTagValue;
		CurrentIO->Tags->GetValue(PCGExGraph::Tag_Cluster, CurrentTagValue))
	{
		FString UpdatedClusterTag;
		CurrentIO->Tags->Set(PCGExGraph::Tag_Cluster, CurrentIO->GetOutIn()->UID, UpdatedClusterTag);
		TaggedEdges = InputDictionary->GetEntries(CurrentTagValue);
		if (!TaggedEdges->Entries.IsEmpty())
		{
			for (const PCGExData::FPointIO* EdgeIO : TaggedEdges->Entries) { EdgeIO->Tags->Set(PCGExGraph::Tag_Cluster, UpdatedClusterTag); }
		}
		else { TaggedEdges = nullptr; }
	}
	else { TaggedEdges = nullptr; }

	if (TaggedEdges)
	{
		CurrentIO->CreateInKeys();

		PCGExGraph::GetRemappedIndices(*CurrentIO, PCGExGraph::Tag_EdgeIndex, NodeIndicesMap);
		EdgeNumReader = new PCGEx::TFAttributeReader<int32>(PCGExGraph::Tag_EdgesNum);
		EdgeNumReader->Bind(*CurrentIO);
	}

	return true;
}

bool FPCGExEdgesProcessorContext::AdvanceEdges(bool bBuildCluster)
{

	PCGEX_DELETE(CurrentCluster);
	
	if (CurrentEdges) { CurrentEdges->Cleanup(); }

	if (TaggedEdges && TaggedEdges->Entries.IsValidIndex(++CurrentEdgesIndex))
	{
		CurrentEdges = TaggedEdges->Entries[CurrentEdgesIndex];
		CurrentEdges->CreateInKeys();

		if (!bBuildCluster) { return true; }

		CurrentCluster = new PCGExCluster::FCluster();

		if (!CurrentCluster->BuildFrom(
			*CurrentEdges,
			CurrentIO->GetIn()->GetPoints(),
			NodeIndicesMap,
			EdgeNumReader->Values))
		{
			// Bad cluster/edges.
			PCGEX_DELETE(CurrentCluster)
		}

		return true;
	}

	CurrentEdges = nullptr;
	return false;
}

void FPCGExEdgesProcessorContext::OutputPointsAndEdges()
{
	MainPoints->OutputTo(this);
	MainEdges->OutputTo(this);
}

PCGEX_INITIALIZE_CONTEXT(EdgesProcessor)

bool FPCGExEdgesProcessorElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(EdgesProcessor)

	if (Context->MainEdges->IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing Edges."));
		return false;
	}

	return true;
}

FPCGContext* FPCGExEdgesProcessorElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	FPCGExPointsProcessorElementBase::InitializeContext(InContext, InputData, SourceComponent, Node);

	PCGEX_CONTEXT_AND_SETTINGS(EdgesProcessor)

	Context->InputDictionary = new PCGExData::FPointIOTaggedDictionary(PCGExGraph::Tag_Cluster);
	Context->MainPoints->ForEach(
		[&](PCGExData::FPointIO& PointIO, int32)
		{
			if (!PCGExGraph::IsPointDataVtxReady(PointIO.GetIn()))
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("A Vtx input has not vtx metadata and will be discarded."));
				PointIO.Disable();
				return;
			}
			Context->InputDictionary->CreateKey(PointIO);
		});

	Context->MainEdges = new PCGExData::FPointIOGroup();
	Context->MainEdges->DefaultOutputLabel = PCGExGraph::OutputEdgesLabel;
	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExGraph::SourceEdgesLabel);
	Context->MainEdges->Initialize(Context, Sources, Settings->GetEdgeOutputInitMode());

	Context->MainEdges->ForEach(
		[&](PCGExData::FPointIO& PointIO, int32)
		{
			if (!PCGExGraph::IsPointDataEdgeReady(PointIO.GetIn()))
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("An Edges input has no edge metadata and will be discarded."));
				PointIO.Disable();
				return;
			}
			Context->InputDictionary->TryAddEntry(PointIO);
		});

	return Context;
}

#undef LOCTEXT_NAMESPACE
