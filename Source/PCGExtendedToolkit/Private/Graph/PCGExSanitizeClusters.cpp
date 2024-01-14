// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExSanitizeClusters.h"

#include "IPCGExDebug.h"
#include "Data/PCGExGraphParamsData.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface


PCGExData::EInit UPCGExSanitizeClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

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

bool UPCGExSanitizeClustersSettings::GetCacheAllClusters() const { return false; }

#pragma endregion

FPCGExSanitizeClustersContext::~FPCGExSanitizeClustersContext()
{
	PCGEX_TERMINATE_ASYNC

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
		CurrentEdges->CreateInKeys();
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

PCGEX_INITIALIZE_CONTEXT(SanitizeClusters)

bool FPCGExSanitizeClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SanitizeClusters)

	if (Context->MainEdges->IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing Edges."));
		return false;
	}

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

	PCGEX_CONTEXT(SanitizeClusters)

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
				//PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points have no associated edges."));
				Context->CurrentIO->Disable();
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
			}
			else
			{
				PCGEx::TFAttributeReader<int32>* IndexReader = new PCGEx::TFAttributeReader<int32>(PCGExGraph::Tag_EdgeIndex);
				if (!IndexReader->Bind(*Context->CurrentIO))
				{
					PCGEX_DELETE(IndexReader)
					Context->CurrentIO->Disable();
					Context->SetState(PCGExMT::State_ReadyForNextPoints);
					return false;
				}

				Context->CachedPointIndices.Reserve(IndexReader->Values.Num());
				for (int i = 0; i < IndexReader->Values.Num(); i++) { Context->CachedPointIndices.Add(IndexReader->Values[i], i); }
				PCGEX_DELETE(IndexReader)
				
				Context->SetState(PCGExGraph::State_ReadyForNextEdges);
			}
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		PCGEX_DELETE(Context->GraphBuilder)
		
		if (!Context->AdvanceEdges()) { Context->SetState(PCGExMT::State_ReadyForNextPoints); }
		else
		{
			PCGExData::FPointIO& EdgeIO = *Context->CurrentEdges;

			Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, 6);
			Context->GraphBuilder->EnablePointsPruning();

			PCGEx::TFAttributeReader<int32>* StartIndexReader = new PCGEx::TFAttributeReader<int32>(PCGExGraph::Tag_EdgeStart);
			if (!StartIndexReader->Bind(const_cast<PCGExData::FPointIO&>(InEdges)))
			{
				PCGEX_DELETE(StartIndexReader)
				return false;
			}

			PCGEx::TFAttributeReader<int32>* EndIndexReader = new PCGEx::TFAttributeReader<int32>(PCGExGraph::Tag_EdgeEnd);
			if (!EndIndexReader->Bind(const_cast<PCGExData::FPointIO&>(InEdges)))
			{
				PCGEX_DELETE(EndIndexReader)
				return false;
			}
			
			const TArray<FPCGPoint>& InEdgePoints = Context->CurrentEdges->GetIn()->GetPoints();
			for(int i = 0; i < InEdgePoints.Num(); i++)
			{
				
				if(Context->GraphBuilder->Graph->InsertEdges(Edges))
			}

			PCGEX_DELETE(StartIndexReader)
			PCGEX_DELETE(EndIndexReader)

			Context->GraphBuilder->Compile(Context);
			Context->SetAsyncState(PCGExGraph::State_WritingClusters);
			
			Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
		}
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone()) { Context->OutputPointsAndEdges(); }

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
