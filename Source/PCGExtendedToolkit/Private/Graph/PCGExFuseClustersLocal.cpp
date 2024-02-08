// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFuseClustersLocal.h"

#include "Data/PCGExGraphDefinition.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExFuseClustersLocalSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }
PCGExData::EInit UPCGExFuseClustersLocalSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

#pragma endregion

FPCGExFuseClustersLocalContext::~FPCGExFuseClustersLocalContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(GraphBuilder)
	PCGEX_DELETE(PointEdgeIntersections)
	PCGEX_DELETE(EdgeEdgeIntersections)
}

PCGEX_INITIALIZE_ELEMENT(FuseClustersLocal)

bool FPCGExFuseClustersLocalElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FuseClustersLocal)

	PCGEX_FWD(PointPointSettings)
	PCGEX_FWD(PointEdgeIntersection)
	PCGEX_FWD(EdgeEdgeIntersection)

	Context->PointPointSettings.FuseSettings.Init();
	Context->PointEdgeIntersection.MakeSafeForTolerance(Context->PointPointSettings.FuseSettings.Tolerance);
	Context->EdgeEdgeIntersection.MakeSafeForTolerance(Context->PointEdgeIntersection.FuseSettings.Tolerance);
	Context->EdgeEdgeIntersection.Init();

	PCGEX_FWD(GraphBuilderSettings)

	return true;
}

bool FPCGExFuseClustersLocalElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFuseClustersLocalElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FuseClustersLocal)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		PCGEX_DELETE(Context->LooseGraph)
		PCGEX_DELETE(Context->GraphBuilder)
		PCGEX_DELETE(Context->PointEdgeIntersections)
		PCGEX_DELETE(Context->EdgeEdgeIntersections)

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges) { return false; }

			Context->LooseGraph = new PCGExGraph::FLooseGraph(Context->PointPointSettings.FuseSettings);
			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		if (Context->CurrentEdges) { Context->CurrentEdges->Cleanup(); }
		if (!Context->AdvanceEdges(false))
		{
			Context->SetState(PCGExGraph::State_ProcessingGraph);
			return false;
		}

		Context->CurrentEdges->CreateInKeys();
		
		// Insert current cluster into loose graph
		Context->GetAsyncManager()->Start<FPCGExInsertLoosePointsTask>(
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
		TArray<FPCGPoint>& MutablePoints = Context->CurrentIO->GetOut()->GetMutablePoints();

		auto Initialize = [&]() { MutablePoints.SetNum(NumLooseNodes); };

		auto ProcessNode = [&](int32 Index)
		{
			PCGExGraph::FLooseNode* LooseNode = Context->LooseGraph->Nodes[Index];
			MutablePoints[Index].Transform.SetLocation(LooseNode->UpdateCenter(Context->MainPoints));
		};

		if (!Context->Process(Initialize, ProcessNode, NumLooseNodes)) { return false; }

		TArray<PCGExGraph::FUnsignedEdge> UniqueEdges;
		Context->LooseGraph->GetUniqueEdges(UniqueEdges);
		PCGEX_DELETE(Context->LooseGraph)

		Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, &Context->GraphBuilderSettings, 6, Context->MainEdges);
		Context->GraphBuilder->Graph->InsertEdges(UniqueEdges);

		UniqueEdges.Empty();

		if (Settings->bDoPointEdgeIntersection)
		{
			Context->PointEdgeIntersections = new PCGExGraph::FPointEdgeIntersections(Context->GraphBuilder->Graph, Context->CurrentIO, Context->PointEdgeIntersection);
			Context->PointEdgeIntersections->FindIntersections(Context);
			Context->SetAsyncState(PCGExGraph::State_FindingPointEdgeIntersections);
		}
		else if (Settings->bDoEdgeEdgeIntersection)
		{
			Context->EdgeEdgeIntersections = new PCGExGraph::FEdgeEdgeIntersections(Context->GraphBuilder->Graph, Context->CurrentIO, Context->EdgeEdgeIntersection);
			Context->EdgeEdgeIntersections->FindIntersections(Context);
			Context->SetAsyncState(PCGExGraph::State_FindingEdgeEdgeIntersections);
		}
		else
		{
			Context->SetAsyncState(PCGExGraph::State_WritingClusters);
		}
	}

	if (Context->IsState(PCGExGraph::State_FindingPointEdgeIntersections))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		Context->PointEdgeIntersections->Insert(); // TODO : Async?

		if (Settings->bDoEdgeEdgeIntersection)
		{
			Context->EdgeEdgeIntersections = new PCGExGraph::FEdgeEdgeIntersections(Context->GraphBuilder->Graph, Context->CurrentIO, Context->EdgeEdgeIntersection);
			Context->EdgeEdgeIntersections->FindIntersections(Context);
			Context->SetAsyncState(PCGExGraph::State_FindingEdgeEdgeIntersections);
		}
		else
		{
			Context->SetAsyncState(PCGExGraph::State_WritingClusters);
		}
	}

	if (Context->IsState(PCGExGraph::State_FindingEdgeEdgeIntersections))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		Context->EdgeEdgeIntersections->Insert(); // TODO : Async?

		Context->SetAsyncState(PCGExGraph::State_WritingClusters);
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
		if (Context->GraphBuilder->bCompiledSuccessfully) { Context->GraphBuilder->Write(Context); }
		
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
