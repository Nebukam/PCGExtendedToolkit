// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExSanitizeClusters.h"

#include "IPCGExDebug.h"
#include "Data/PCGExGraphParamsData.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExSanitizeClustersSettings::GetMainOutputInitMode() const { return bPruneIsolatedPoints ? PCGExData::EInit::NewOutput : PCGExData::EInit::DuplicateInput; }
PCGExData::EInit UPCGExSanitizeClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

#pragma endregion

FPCGExSanitizeClustersContext::~FPCGExSanitizeClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(StartIndexReader)
	PCGEX_DELETE(EndIndexReader)

	PCGEX_DELETE(GraphBuilder)
}

PCGEX_INITIALIZE_ELEMENT(SanitizeClusters)

bool FPCGExSanitizeClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SanitizeClusters)

	Context->MinClusterSize = Settings->bRemoveSmallClusters ? FMath::Max(1, Settings->MinClusterSize) : 1;
	Context->MaxClusterSize = Settings->bRemoveBigClusters ? FMath::Max(1, Settings->MaxClusterSize) : TNumericLimits<int32>::Max();

	return true;
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

		Context->SetState(PCGExGraph::State_ProcessingEdges);
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

			const int32* NodeStartPtr = Context->NodeIndicesMap.Find(Context->StartIndexReader->Values[EdgeIndex]);
			const int32* NodeEndPtr = Context->NodeIndicesMap.Find(Context->EndIndexReader->Values[EdgeIndex]);

			if (!NodeStartPtr || !NodeEndPtr) { return; }

			const int32 NodeStart = *NodeStartPtr;
			const int32 NodeEnd = *NodeEndPtr;

			if (!InNodePoints.IsValidIndex(NodeStart) ||
				!InNodePoints.IsValidIndex(NodeEnd) ||
				NodeStart == NodeEnd) { return;; }

			if (!Context->GraphBuilder->Graph->InsertEdge(NodeStart, NodeEnd, NewEdge)) { return; }
			NewEdge.PointIndex = EdgeIndex; // Tag edge since it's a new insertion
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
