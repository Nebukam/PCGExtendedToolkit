// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFuseClusters.h"

#include "Data/PCGExGraphParamsData.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExFuseClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }
PCGExData::EInit UPCGExFuseClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

#pragma endregion

FPCGExFuseClustersContext::~FPCGExFuseClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(LooseGraph)
	PCGEX_DELETE(GraphBuilder)
	PCGEX_DELETE(Intersections)
}

PCGEX_INITIALIZE_ELEMENT(FuseClusters)

bool FPCGExFuseClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FuseClusters)

	PCGEX_FWD(FuseDistance)
	PCGEX_FWD(PointEdgeTolerance)
	PCGEX_FWD(EdgeEdgeTolerance)

	Context->PointEdgeTolerance = FMath::Min(Context->PointEdgeTolerance, Context->FuseDistance * 0.5);
	Context->EdgeEdgeTolerance = FMath::Min(Context->EdgeEdgeTolerance, Context->PointEdgeTolerance * 0.5);

	PCGEX_FWD(GraphBuilderSettings)

	Context->LooseGraph = new PCGExGraph::FLooseGraph(Settings->FuseDistance);

	return true;
}

bool FPCGExFuseClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFuseClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FuseClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			Context->ConsolidatedPoints = &Context->MainPoints->Emplace_GetRef(PCGExData::EInit::NewOutput);
			Context->SetState(PCGExGraph::State_ProcessingGraph);
		}
		else
		{
			if (!Context->TaggedEdges) { return false; }
			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		if (Context->CurrentEdges) { Context->CurrentEdges->Cleanup(); }
		if (!Context->AdvanceEdges(false))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		Context->CurrentEdges->CreateInKeys();

		// Insert current cluster into loose graph
		Context->GetAsyncManager()->Start<FPCGExFuseClustersInsertLoosePointsTask>(
			Context->CurrentIO->IOIndex, Context->CurrentIO,
			Context->LooseGraph, Context->CurrentEdges, &Context->NodeIndicesMap);

		Context->SetAsyncState(PCGExGraph::State_ProcessingEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
		Context->SetState(PCGExGraph::State_ReadyForNextEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingGraph))
	{
		const int32 NumLooseNodes = Context->LooseGraph->Nodes.Num();

		auto Initialize = [&]()
		{
			TArray<FPCGPoint>& MutablePoints = Context->ConsolidatedPoints->GetOut()->GetMutablePoints();
			MutablePoints.SetNum(NumLooseNodes);
		};

		auto ProcessNode = [&](int32 Index)
		{
			Context->ConsolidatedPoints->GetMutablePoint(Index).Transform.SetLocation(
				Context->LooseGraph->Nodes[Index]->UpdateCenter(Context->MainPoints));
		};

		if (!Context->Process(Initialize, ProcessNode, NumLooseNodes)) { return false; }

		TArray<PCGExGraph::FUnsignedEdge> UniqueEdges;
		Context->LooseGraph->GetUniqueEdges(UniqueEdges);
		PCGEX_DELETE(Context->LooseGraph)

		Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->ConsolidatedPoints, &Context->GraphBuilderSettings);
		Context->GraphBuilder->Graph->InsertEdges(UniqueEdges);

		UniqueEdges.Empty();

		Context->Intersections = new PCGExGraph::FEdgePointIntersectionList(Context->GraphBuilder->Graph, Context->ConsolidatedPoints, Settings->PointEdgeTolerance);
		Context->Intersections->FindIntersections(Context);

		Context->SetAsyncState(PCGExGraph::State_FindingPointEdgeIntersections);
	}

	if (Context->IsState(PCGExGraph::State_FindingPointEdgeIntersections))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
		Context->Intersections->Insert(); // TODO : Async?
		Context->SetState(PCGExGraph::State_WritingClusters);
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		Context->GraphBuilder->Compile(Context);
		Context->SetAsyncState(PCGExGraph::State_WaitingOnWritingClusters);
		return false;
	}

	if (Context->IsState(PCGExGraph::State_WaitingOnWritingClusters))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		if (Context->GraphBuilder->bCompiledSuccessfully)
		{
			Context->GraphBuilder->Write(Context);
			Context->OutputPoints();
		}

		Context->Done();
	}

	return Context->IsDone();
}

bool FPCGExFuseClustersInsertLoosePointsTask::ExecuteTask()
{
	TArray<PCGExGraph::FIndexedEdge> IndexedEdges;
	if (!BuildIndexedEdges(*EdgeIO, *NodeIndicesMap, IndexedEdges, true) ||
		IndexedEdges.IsEmpty()) { return false; }

	const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
	for (const PCGExGraph::FIndexedEdge& Edge : IndexedEdges)
	{
		Graph->CreateBridge(
			InPoints[Edge.Start].Transform.GetLocation(), TaskIndex, Edge.Start,
			InPoints[Edge.End].Transform.GetLocation(), TaskIndex, Edge.End);
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
