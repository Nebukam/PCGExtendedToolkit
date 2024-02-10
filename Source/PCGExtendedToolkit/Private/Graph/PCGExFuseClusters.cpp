// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFuseClusters.h"

#include "Data/PCGExGraphDefinition.h"

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
	PCGEX_DELETE(PointEdgeIntersections)
	PCGEX_DELETE(EdgeEdgeIntersections)
}

PCGEX_INITIALIZE_ELEMENT(FuseClusters)

bool FPCGExFuseClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FuseClusters)

	PCGEX_FWD(PointPointSettings)
	PCGEX_FWD(PointEdgeIntersection)
	PCGEX_FWD(EdgeEdgeIntersection)

	Context->PointPointSettings.FuseSettings.Init();
	Context->EdgeEdgeIntersection.Init();

	Context->GraphMetadataSettings.Grab(Context, Context->PointPointSettings);
	Context->GraphMetadataSettings.Grab(Context, Context->PointEdgeIntersection);
	Context->GraphMetadataSettings.Grab(Context, Context->EdgeEdgeIntersection);

	//Context->PointEdgeIntersection.MakeSafeForTolerance(Context->PointPointSettings.FuseSettings.Tolerance);
	//Context->EdgeEdgeIntersection.MakeSafeForTolerance(Context->PointEdgeIntersection.FuseSettings.Tolerance);

	PCGEX_FWD(GraphBuilderSettings)

	Context->LooseGraph = new PCGExGraph::FLooseGraph(Context->PointPointSettings.FuseSettings);

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
		Context->GetAsyncManager()->Start<FPCGExInsertLooseNodesTask>(
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
		TArray<FPCGPoint>& MutablePoints = Context->ConsolidatedPoints->GetOut()->GetMutablePoints();

		auto Initialize = [&]() { MutablePoints.SetNum(NumLooseNodes); };

		auto ProcessNode = [&](int32 Index)
		{
			PCGExGraph::FLooseNode* LooseNode = Context->LooseGraph->Nodes[Index];
			MutablePoints[Index].Transform.SetLocation(LooseNode->UpdateCenter(Context->MainPoints));
		};

		if (!Context->Process(Initialize, ProcessNode, NumLooseNodes)) { return false; }

		Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->ConsolidatedPoints, &Context->GraphBuilderSettings, 6, Context->MainEdges);
		
		TArray<PCGExGraph::FUnsignedEdge> UniqueEdges;
		Context->LooseGraph->GetUniqueEdges(UniqueEdges);
		Context->LooseGraph->WriteMetadata(Context->GraphBuilder->Graph->NodeMetadata);
		PCGEX_DELETE(Context->LooseGraph)
		
		Context->GraphBuilder->Graph->InsertEdges(UniqueEdges, -1);
		UniqueEdges.Empty();

		if (Settings->bDoPointEdgeIntersection)
		{
			Context->PointEdgeIntersections = new PCGExGraph::FPointEdgeIntersections(Context->GraphBuilder->Graph, Context->ConsolidatedPoints, Context->PointEdgeIntersection);
			Context->PointEdgeIntersections->FindIntersections(Context);
			Context->SetAsyncState(PCGExGraph::State_FindingPointEdgeIntersections);
		}
		else if (Settings->bDoEdgeEdgeIntersection)
		{
			Context->EdgeEdgeIntersections = new PCGExGraph::FEdgeEdgeIntersections(Context->GraphBuilder->Graph, Context->ConsolidatedPoints, Context->EdgeEdgeIntersection);
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
		PCGEX_DELETE(Context->PointEdgeIntersections)

		if (Settings->bDoEdgeEdgeIntersection)
		{
			Context->EdgeEdgeIntersections = new PCGExGraph::FEdgeEdgeIntersections(Context->GraphBuilder->Graph, Context->ConsolidatedPoints, Context->EdgeEdgeIntersection);
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
		PCGEX_DELETE(Context->EdgeEdgeIntersections)

		Context->SetAsyncState(PCGExGraph::State_WritingClusters);
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		Context->GraphBuilder->Compile(Context, &Context->GraphMetadataSettings);
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

#undef LOCTEXT_NAMESPACE
