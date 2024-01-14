// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExSanitizeClusters.h"

#include "IPCGExDebug.h"
#include "Data/PCGExGraphParamsData.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface


PCGExData::EInit UPCGExSanitizeClustersSettings::GetMainOutputInitMode() const { return bPruneIsolatedPoints ? PCGExData::EInit::NewOutput : PCGExData::EInit::DuplicateInput; }

FName UPCGExSanitizeClustersSettings::GetMainInputLabel() const { return PCGExGraph::SourceVerticesLabel; }
FName UPCGExSanitizeClustersSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

TArray<FPCGPinProperties> UPCGExSanitizeClustersSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	FPCGPinProperties& PinEdgesInput = PinProperties.Emplace_GetRef(PCGExGraph::SourceEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinEdgesInput.Tooltip = FTEXT("Edges associated with the main input points");
#endif

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExSanitizeClustersSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	FPCGPinProperties& PinEdgesOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinEdgesOutput.Tooltip = FTEXT("Edges associated with the main output points");
#endif

	return PinProperties;
}

#pragma endregion

FPCGExSanitizeClustersContext::~FPCGExSanitizeClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(StartIndexReader)
	PCGEX_DELETE(EndIndexReader)

	PCGEX_DELETE(GraphBuilder)
	PCGEX_DELETE(InputDictionary)
	PCGEX_DELETE(MainEdges)

	CachedPointIndices.Empty();
}


bool FPCGExSanitizeClustersContext::AdvancePointsIO()
{
	CurrentEdgesIndex = -1;

	if (!FPCGExPointsProcessorContext::AdvancePointsIO()) { return false; }

	if (FString CurrentTagValue;
		CurrentIO->Tags->GetValue(PCGExGraph::Tag_Cluster, CurrentTagValue))
	{
		TaggedEdges = InputDictionary->GetEntries(CurrentTagValue);
		if (TaggedEdges->Entries.IsEmpty()) { TaggedEdges = nullptr; }
	}
	else { TaggedEdges = nullptr; }

	if (TaggedEdges) { CurrentIO->CreateInKeys(); }

	return true;
}

bool FPCGExSanitizeClustersContext::AdvanceEdges()
{
	if (CurrentEdges) { CurrentEdges->Cleanup(); }

	if (TaggedEdges && TaggedEdges->Entries.IsValidIndex(++CurrentEdgesIndex))
	{
		CurrentEdges = TaggedEdges->Entries[CurrentEdgesIndex];
		return true;
	}

	CurrentEdges = nullptr;
	return false;
}

void FPCGExSanitizeClustersContext::OutputPointsAndEdges()
{
	MainPoints->OutputTo(this);
	MainEdges->OutputTo(this);
}

PCGEX_INITIALIZE_ELEMENT(SanitizeClusters)

bool FPCGExSanitizeClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SanitizeClusters)

	if (Context->MainEdges->IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing Edges."));
		return false;
	}

	Context->MinClusterSize = Settings->bRemoveSmallClusters ? FMath::Max(1, Settings->MinClusterSize) : 1;
	Context->MaxClusterSize = Settings->bRemoveBigClusters ? FMath::Max(1, Settings->MaxClusterSize) : TNumericLimits<int32>::Max();

	return true;
}

FPCGContext* FPCGExSanitizeClustersElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	FPCGExPointsProcessorElementBase::InitializeContext(InContext, InputData, SourceComponent, Node);

	PCGEX_CONTEXT_AND_SETTINGS(SanitizeClusters)

	Context->InputDictionary = new PCGExData::FPointIOTaggedDictionary(PCGExGraph::Tag_Cluster);
	Context->MainPoints->ForEach(
		[&](PCGExData::FPointIO& PointIO, int32)
		{
			Context->InputDictionary->CreateKey(PointIO);
		});

	Context->MainEdges = new PCGExData::FPointIOGroup();
	Context->MainEdges->DefaultOutputLabel = PCGExGraph::OutputEdgesLabel;

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExGraph::SourceEdgesLabel);
	Context->MainEdges->Initialize(Context, Sources, PCGExData::EInit::NoOutput);

	Context->MainEdges->ForEach(
		[&](PCGExData::FPointIO& PointIO, int32)
		{
			Context->InputDictionary->TryAddEntry(PointIO);
		});

	return Context;
}

bool FPCGExSanitizeClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSanitizeClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SanitizeClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		Context->CachedPointIndices.Empty();

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges)
			{
				Context->CurrentIO->Disable();
				return false;
			}

			PCGEx::FAttributesInfos* NodesInfos = PCGEx::FAttributesInfos::Get(Context->CurrentIO->GetIn());
			if (!NodesInfos->Contains(PCGExGraph::Tag_EdgeIndex, EPCGMetadataTypes::Integer32) ||
				!NodesInfos->Contains(PCGExGraph::Tag_EdgesNum, EPCGMetadataTypes::Integer32))
			{
				Context->CurrentIO->Disable();
				PCGEX_DELETE(NodesInfos)
				return false;
			}

			PCGEX_DELETE(NodesInfos)

			PCGExGraph::GetRemappedIndices(*Context->CurrentIO, PCGExGraph::Tag_EdgeIndex, Context->CachedPointIndices);

			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		PCGEX_DELETE(Context->GraphBuilder)

		if (!Context->AdvanceEdges()) { Context->SetState(PCGExMT::State_ReadyForNextPoints); }
		else
		{
			PCGEx::FAttributesInfos* EdgeInfos = PCGEx::FAttributesInfos::Get(Context->CurrentEdges->GetIn());
			if (!EdgeInfos->Contains(PCGExGraph::Tag_EdgeStart, EPCGMetadataTypes::Integer32) ||
				!EdgeInfos->Contains(PCGExGraph::Tag_EdgeEnd, EPCGMetadataTypes::Integer32))
			{
				PCGEX_DELETE(EdgeInfos)
				return false;
			}

			PCGEX_DELETE(EdgeInfos)
			Context->SetState(PCGExGraph::State_ProcessingEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		const TArray<FPCGPoint>& InEdgePoints = Context->CurrentEdges->GetIn()->GetPoints();
		const TArray<FPCGPoint>& InNodePoints = Context->CurrentIO->GetIn()->GetPoints();

		auto Initialize = [&]()
		{
			Context->CurrentEdges->CreateInKeys();

			PCGExData::FPointIO& EdgeIO = *Context->CurrentEdges;

			Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, 6, &EdgeIO);
			if (Settings->bPruneIsolatedPoints) { Context->GraphBuilder->EnablePointsPruning(); }

			Context->StartIndexReader = new PCGEx::TFAttributeReader<int32>(PCGExGraph::Tag_EdgeStart);
			Context->EndIndexReader = new PCGEx::TFAttributeReader<int32>(PCGExGraph::Tag_EdgeEnd);

			Context->StartIndexReader->Bind(EdgeIO);
			Context->EndIndexReader->Bind(EdgeIO);
		};

		auto InsertEdge = [&](int32 EdgeIndex)
		{
			PCGExGraph::FIndexedEdge NewEdge = PCGExGraph::FIndexedEdge{};

			const int32* NodeStartPtr = Context->CachedPointIndices.Find(Context->StartIndexReader->Values[EdgeIndex]);
			const int32* NodeEndPtr = Context->CachedPointIndices.Find(Context->EndIndexReader->Values[EdgeIndex]);

			if (!NodeStartPtr || !NodeEndPtr) { return; }

			const int32 NodeStart = *NodeStartPtr;
			const int32 NodeEnd = *NodeEndPtr;

			if (!InNodePoints.IsValidIndex(NodeStart) ||
				!InNodePoints.IsValidIndex(NodeEnd) ||
				NodeStart == NodeEnd) { return;; }

			if (!Context->GraphBuilder->Graph->InsertEdge(NodeStart, NodeEnd, NewEdge)) { return; }
			NewEdge.TaggedIndex = EdgeIndex; // Tag edge since it's a new insertion
		};

		if (!Context->Process(Initialize, InsertEdge, InEdgePoints.Num())) { return false; }

		PCGEX_DELETE(Context->StartIndexReader)
		PCGEX_DELETE(Context->EndIndexReader)

		Context->GraphBuilder->Compile(Context, Context->MinClusterSize, Context->MaxClusterSize);
		Context->SetAsyncState(PCGExGraph::State_WritingClusters);
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
		if (Context->GraphBuilder->bCompiledSuccessfully) { Context->GraphBuilder->Write(Context); }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->OutputPointsAndEdges();
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
