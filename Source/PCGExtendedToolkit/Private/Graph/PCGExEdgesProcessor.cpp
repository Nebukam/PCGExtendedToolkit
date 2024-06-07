// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExEdgesProcessor.h"

#include "Data/PCGExGraphDefinition.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface


PCGExData::EInit UPCGExEdgesProcessorSettings::GetMainOutputInitMode() const { return PCGExData::EInit::Forward; }

FName UPCGExEdgesProcessorSettings::GetMainInputLabel() const { return PCGExGraph::SourceVerticesLabel; }
FName UPCGExEdgesProcessorSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGExData::EInit UPCGExEdgesProcessorSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::Forward; }

bool UPCGExEdgesProcessorSettings::RequiresDeterministicClusters() const { return false; }

bool UPCGExEdgesProcessorSettings::GetMainAcceptMultipleData() const { return true; }

TArray<FPCGPinProperties> UPCGExEdgesProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::SourceEdgesLabel, "Edges associated with the main input points", Required, {})
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExEdgesProcessorSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Edges associated with the main output points", Required, {})
	return PinProperties;
}

#pragma endregion

FPCGExEdgesProcessorContext::~FPCGExEdgesProcessorContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(InputDictionary)
	PCGEX_DELETE(MainEdges)
	PCGEX_DELETE(CurrentCluster)
	PCGEX_DELETE(ClusterProjection)

	NodeIndicesMap.Empty();
	ProjectionSettings.Cleanup();
}


bool FPCGExEdgesProcessorContext::AdvancePointsIO()
{
	PCGEX_DELETE(EdgeNumReader)
	PCGEX_DELETE(CurrentCluster)
	PCGEX_DELETE(ClusterProjection)

	CurrentEdgesIndex = -1;
	NodeIndicesMap.Empty();

	if (!FPCGExPointsProcessorContext::AdvancePointsIO()) { return false; }

	if (FString CurrentPairId;
		CurrentIO->Tags->GetValue(PCGExGraph::TagStr_ClusterPair, CurrentPairId))
	{
		FString OutId;
		CurrentIO->Tags->Set(PCGExGraph::TagStr_ClusterPair, CurrentIO->GetOutIn()->UID, OutId);

		TaggedEdges = InputDictionary->GetEntries(CurrentPairId);
		if (TaggedEdges && !TaggedEdges->Entries.IsEmpty())
		{
			for (const PCGExData::FPointIO* EdgeIO : TaggedEdges->Entries) { EdgeIO->Tags->Set(PCGExGraph::TagStr_ClusterPair, OutId); }
		}
		else { TaggedEdges = nullptr; }
	}
	else { TaggedEdges = nullptr; }

	if (TaggedEdges)
	{
		CurrentIO->CreateInKeys();

		ProjectionSettings.Init(CurrentIO);

		PCGExGraph::GetRemappedIndices(*CurrentIO, PCGExGraph::Tag_EdgeIndex, NodeIndicesMap);
		EdgeNumReader = new PCGEx::TFAttributeReader<int32>(PCGExGraph::Tag_EdgesNum);
		EdgeNumReader->Bind(*CurrentIO);
	}

	return true;
}

bool FPCGExEdgesProcessorContext::AdvanceEdges(const bool bBuildCluster)
{
	PCGEX_DELETE(CurrentCluster)
	PCGEX_DELETE(ClusterProjection)

	if (bBuildCluster && CurrentEdges) { CurrentEdges->CleanupKeys(); }

	if (TaggedEdges && TaggedEdges->Entries.IsValidIndex(++CurrentEdgesIndex))
	{
		CurrentEdges = TaggedEdges->Entries[CurrentEdgesIndex];

		if (!bBuildCluster) { return true; }

		CurrentEdges->CreateInKeys();
		CurrentCluster = new PCGExCluster::FCluster();

		if (!CurrentCluster->BuildFrom(
			*CurrentEdges,
			CurrentIO->GetIn()->GetPoints(),
			NodeIndicesMap,
			EdgeNumReader->Values, bDeterministicClusters))
		{
			// Bad cluster/edges.
			PCGEX_DELETE(CurrentCluster)
		}
		else
		{
			CurrentCluster->PointsIO = CurrentIO;
			CurrentCluster->EdgesIO = CurrentEdges;
		}

		return true;
	}

	CurrentEdges = nullptr;
	return false;
}

bool FPCGExEdgesProcessorContext::ProjectCluster()
{
	const int32 NumNodes = CurrentCluster->Nodes.Num();

	auto Initialize = [&]()
	{
		PCGEX_DELETE(ClusterProjection)
		ClusterProjection = new PCGExCluster::FClusterProjection(CurrentCluster, &ProjectionSettings);
	};

	auto ProjectSinglePoint = [&](const int32 Index) { ClusterProjection->Nodes[Index].Project(CurrentCluster, &ProjectionSettings); };

	return Process(Initialize, ProjectSinglePoint, NumNodes);
}

void FPCGExEdgesProcessorContext::OutputPointsAndEdges()
{
	MainPoints->OutputTo(this);
	MainEdges->OutputTo(this);
}

PCGEX_INITIALIZE_CONTEXT(EdgesProcessor)

void FPCGExEdgesProcessorElement::DisabledPassThroughData(FPCGContext* Context) const
{
	FPCGExPointsProcessorElementBase::DisabledPassThroughData(Context);

	//Forward main edges
	TArray<FPCGTaggedData> EdgesSources = Context->InputData.GetInputsByPin(PCGExGraph::SourceEdgesLabel);
	for (const FPCGTaggedData& TaggedData : EdgesSources)
	{
		FPCGTaggedData& TaggedDataCopy = Context->OutputData.TaggedData.Emplace_GetRef();
		TaggedDataCopy.Data = TaggedData.Data;
		TaggedDataCopy.Tags.Append(TaggedData.Tags);
		TaggedDataCopy.Pin = PCGExGraph::OutputEdgesLabel;
	}
}

bool FPCGExEdgesProcessorElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(EdgesProcessor)

	Context->bDeterministicClusters = Settings->RequiresDeterministicClusters();

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

	if (!Settings->bEnabled) { return Context; }

	Context->InputDictionary = new PCGExData::FPointIOTaggedDictionary(PCGExGraph::TagStr_ClusterPair);
	Context->MainPoints->ForEach(
		[&](PCGExData::FPointIO& PointIO, int32)
		{
			if (!PCGExGraph::IsPointDataVtxReady(PointIO.GetIn()))
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("A Vtx input has no metadata and will be discarded."));
				PointIO.Disable();
				return;
			}
			if (!Context->InputDictionary->CreateKey(PointIO))
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("At least two Vtx inputs share the same PCGEx/Cluster tag. Only one will be processed."));
				PointIO.Disable();
			}
		});

	Context->MainEdges = new PCGExData::FPointIOCollection();
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
