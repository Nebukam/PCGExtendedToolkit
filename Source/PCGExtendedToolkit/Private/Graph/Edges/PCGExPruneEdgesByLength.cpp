// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExPruneEdgesByLength.h"

#include "IPCGExDebug.h"
#include "Data/PCGExGraphParamsData.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExPruneEdgesByLengthSettings::GetMainOutputInitMode() const { return bPruneIsolatedPoints ? PCGExData::EInit::NewOutput : PCGExData::EInit::DuplicateInput; }
PCGExData::EInit UPCGExPruneEdgesByLengthSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

#pragma endregion

FPCGExPruneEdgesByLengthContext::~FPCGExPruneEdgesByLengthContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(StartIndexReader)
	PCGEX_DELETE(EndIndexReader)

	PCGEX_DELETE(GraphBuilder)

	Edges.Empty();
	EdgeLength.Empty();
}

PCGEX_INITIALIZE_ELEMENT(PruneEdgesByLength)

bool FPCGExPruneEdgesByLengthElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PruneEdgesByLength)

	Context->MinClusterSize = Settings->bRemoveSmallClusters ? FMath::Max(1, Settings->MinClusterSize) : 1;
	Context->MaxClusterSize = Settings->bRemoveBigClusters ? FMath::Max(1, Settings->MaxClusterSize) : TNumericLimits<int32>::Max();

	return true;
}

bool FPCGExPruneEdgesByLengthElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPruneEdgesByLengthElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PruneEdgesByLength)

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

		Context->MinEdgeLength = TNumericLimits<double>::Max();
		Context->MaxEdgeLength = TNumericLimits<double>::Min();

		const TArray<FPCGPoint>& InNodePoints = Context->CurrentIO->GetIn()->GetPoints();

		PCGExData::FPointIO& EdgeIO = *Context->CurrentEdges;
		const int32 NumEdges = EdgeIO.GetNum();

		Context->Edges.Reset(NumEdges);
		Context->EdgeLength.Reset(NumEdges);

		EdgeIO.CreateInKeys();

		Context->StartIndexReader = new PCGEx::TFAttributeReader<int32>(PCGExGraph::Tag_EdgeStart);
		Context->EndIndexReader = new PCGEx::TFAttributeReader<int32>(PCGExGraph::Tag_EdgeEnd);

		Context->StartIndexReader->Bind(EdgeIO);
		Context->EndIndexReader->Bind(EdgeIO);

		for (int i = 0; i < NumEdges; i++)
		{
			const int32* NodeStartPtr = Context->NodeIndicesMap.Find(Context->StartIndexReader->Values[i]);
			const int32* NodeEndPtr = Context->NodeIndicesMap.Find(Context->EndIndexReader->Values[i]);

			if (!NodeStartPtr || !NodeEndPtr) { continue; }

			const int32 NodeStart = *NodeStartPtr;
			const int32 NodeEnd = *NodeEndPtr;

			if (!InNodePoints.IsValidIndex(NodeStart) ||
				!InNodePoints.IsValidIndex(NodeEnd) ||
				NodeStart == NodeEnd) { continue; }

			Context->Edges.Emplace(i, NodeStart, NodeEnd, i);
			double EdgeLength = FVector::DistSquared(InNodePoints[NodeStart].Transform.GetLocation(), InNodePoints[NodeEnd].Transform.GetLocation());
			Context->EdgeLength.Add(EdgeLength);

			Context->MinEdgeLength = FMath::Min(Context->MinEdgeLength, EdgeLength);
			Context->MaxEdgeLength = FMath::Max(Context->MaxEdgeLength, EdgeLength);
		}

		PCGEX_DELETE(Context->StartIndexReader)
		PCGEX_DELETE(Context->EndIndexReader)

		
		Context->SetState(PCGExGraph::State_ProcessingEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		const TArray<FPCGPoint>& InEdgePoints = Context->CurrentEdges->GetIn()->GetPoints();
		const TArray<FPCGPoint>& InNodePoints = Context->CurrentIO->GetIn()->GetPoints();

		auto Initialize = [&]()
		{
			PCGExData::FPointIO& EdgeIO = *Context->CurrentEdges;
			
			Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, 6, &EdgeIO);
			if (Settings->bPruneIsolatedPoints) { Context->GraphBuilder->EnablePointsPruning(); }

		};

		//TODO: Compute edge length and min/max first, as well as median etc

		auto InsertEdge = [&](int32 EdgeIndex)
		{
			PCGExGraph::FIndexedEdge& ProcessedEdge = ;
			
			PCGExGraph::FIndexedEdge* NewEdge = nullptr;
			if (!Context->GraphBuilder->Graph->InsertEdge(ProcessedEdge.Start, ProcessedEdge.End, NewEdge)) { return; }
		};

		if (!Context->Process(Initialize, InsertEdge, Context->Edges.Num())) { return false; }

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
