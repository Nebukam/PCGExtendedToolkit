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

bool UPCGExEdgesProcessorSettings::GetCacheAllClusters() const { return false; }

#pragma endregion

FPCGExEdgesProcessorContext::~FPCGExEdgesProcessorContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(InputDictionary)
	PCGEX_DELETE(MainEdges)

	if (bCacheAllClusters)
	{
		CurrentCluster = nullptr;
		PCGEX_DELETE_TARRAY(Clusters)
	}
	else { PCGEX_DELETE(CurrentCluster) }

	RemappedPointsIndices.Empty();
}


bool FPCGExEdgesProcessorContext::AdvancePointsIO()
{
	PCGEX_DELETE_TARRAY(Clusters)
	PCGEX_DELETE(EdgeNumReader)
	CurrentEdgesIndex = -1;
	RemappedPointsIndices.Empty();

	if (!FPCGExPointsProcessorContext::AdvancePointsIO()) { return false; }

	if (FString CurrentTagValue;
		CurrentIO->Tags->GetValue(PCGExGraph::Tag_Cluster, CurrentTagValue))
	{
		TaggedEdges = InputDictionary->GetEntries(CurrentTagValue);
		if (TaggedEdges->Entries.IsEmpty()) { TaggedEdges = nullptr; }
	}
	else { TaggedEdges = nullptr; }

	if (TaggedEdges)
	{
		bool bValidPoints = false;
		CurrentIO->CreateInKeys();

		if (PCGExGraph::GetRemappedIndices(*CurrentIO, PCGExGraph::Tag_EdgeIndex, RemappedPointsIndices))
		{
			EdgeNumReader = new PCGEx::TFAttributeReader<int32>(PCGExGraph::Tag_EdgesNum);
			if (EdgeNumReader->Bind(*CurrentIO)) { bValidPoints = true; }
		}

		if (!bValidPoints)
		{
			PCGEX_DELETE(EdgeNumReader)
			TaggedEdges = nullptr;
		}
	}

	return true;
}

bool FPCGExEdgesProcessorContext::AdvanceEdges()
{
	if (!bCacheAllClusters) { PCGEX_DELETE(CurrentCluster) }

	if (CurrentEdges) { CurrentEdges->Cleanup(); }

	if (TaggedEdges && TaggedEdges->Entries.IsValidIndex(++CurrentEdgesIndex))
	{
		CurrentEdges = TaggedEdges->Entries[CurrentEdgesIndex];
		CurrentEdges->CreateInKeys();

		PCGEx::FAttributesInfos* EdgeInfos = PCGEx::FAttributesInfos::Get(CurrentEdges->GetIn());
		if (EdgeInfos->Contains(PCGExGraph::Tag_EdgeStart, EPCGMetadataTypes::Integer32) &&
			EdgeInfos->Contains(PCGExGraph::Tag_EdgeEnd, EPCGMetadataTypes::Integer32))
		{
			CurrentCluster = new PCGExCluster::FCluster();

			if (!CurrentCluster->BuildFrom(
				*CurrentEdges,
				CurrentIO->GetIn()->GetPoints(),
				RemappedPointsIndices,
				EdgeNumReader->Values))
			{
				// Bad cluster/edges.
				PCGEX_DELETE(CurrentCluster)
				CurrentEdges->Cleanup();
			}
			else if (bCacheAllClusters) { Clusters.Add(CurrentCluster); }
		}

		PCGEX_DELETE(EdgeInfos)
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
			Context->InputDictionary->CreateKey(PointIO);
		});

	Context->MainEdges = new PCGExData::FPointIOGroup();
	Context->MainEdges->DefaultOutputLabel = PCGExGraph::OutputEdgesLabel;
	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExGraph::SourceEdgesLabel);
	Context->MainEdges->Initialize(Context, Sources, Settings->GetEdgeOutputInitMode());

	Context->MainEdges->ForEach(
		[&](PCGExData::FPointIO& PointIO, int32)
		{
			Context->InputDictionary->TryAddEntry(PointIO);
		});

	Context->bCacheAllClusters = Settings->GetCacheAllClusters();

	return Context;
}

#undef LOCTEXT_NAMESPACE
