// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExFindEdgesIntersections.h"

#include "IPCGExDebug.h"
#include "Data/PCGExGraphParamsData.h"
#include "Sampling/PCGExSampling.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExFindEdgesIntersectionsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }
PCGExData::EInit UPCGExFindEdgesIntersectionsSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

#pragma endregion

FPCGExFindEdgesIntersectionsContext::~FPCGExFindEdgesIntersectionsContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(GraphBuilder)
}

PCGEX_INITIALIZE_ELEMENT(FindEdgesIntersections)

bool FPCGExFindEdgesIntersectionsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindEdgesIntersections)
	PCGEX_FWD(CrossingTolerance)

	PCGEX_FWD(GraphBuilderSettings)

	return true;
}

bool FPCGExFindEdgesIntersectionsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindEdgesIntersectionsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FindEdgesIntersections)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		Context->NodeIndicesMap.Empty();

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges) { return false; }
			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		PCGEX_DELETE(Context->GraphBuilder)

		if (!Context->AdvanceEdges(false))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		Context->CurrentEdges->CreateInKeys();
		
		Context->SetState(PCGExGraph::State_ProcessingEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, &Context->GraphBuilderSettings, 6, Context->CurrentEdges);

		//Context->GraphBuilder->Graph->InsertEdges(Context->IndexedEdges);

		//TODO: Insert merged edges 
		
		Context->GraphBuilder->Compile(Context);
		Context->SetAsyncState(PCGExGraph::State_WritingClusters);
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
		if (Context->GraphBuilder->bCompiledSuccessfully)
		{
			
			Context->GraphBuilder->Write(Context);
		}
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->OutputPointsAndEdges();
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
