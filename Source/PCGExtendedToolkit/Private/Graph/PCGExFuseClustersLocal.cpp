// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFuseClustersLocal.h"

#include "Data/Blending/PCGExCompoundBlender.h"
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

	PCGEX_DELETE(CompoundPointsBlender)
	PCGEX_DELETE(CompoundEdgesBlender)
}

PCGEX_INITIALIZE_ELEMENT(FuseClustersLocal)

bool FPCGExFuseClustersLocalElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FuseClustersLocal)

	PCGEX_FWD(PointPointIntersectionSettings)
	PCGEX_FWD(PointEdgeIntersectionSettings)
	PCGEX_FWD(EdgeEdgeIntersectionSettings)

	Context->EdgeEdgeIntersectionSettings.ComputeDot();

	Context->GraphMetadataSettings.Grab(Context, Context->PointPointIntersectionSettings);
	Context->GraphMetadataSettings.Grab(Context, Context->PointEdgeIntersectionSettings);
	Context->GraphMetadataSettings.Grab(Context, Context->EdgeEdgeIntersectionSettings);

	//Context->PointEdgeIntersectionSettings.MakeSafeForTolerance(Context->PointPointSettings.FuseSettings.Tolerance);
	//Context->EdgeEdgeIntersectionSettings.MakeSafeForTolerance(Context->PointEdgeIntersectionSettings.FuseSettings.Tolerance);
	//Context->EdgeEdgeIntersectionSettings.Init();

	PCGEX_FWD(GraphBuilderSettings)

	Context->CompoundPointsBlender = new PCGExDataBlending::FCompoundBlender(const_cast<FPCGExBlendingSettings*>(&Settings->PointsBlendingSettings));
	Context->CompoundPointsBlender->AddSources(*Context->MainPoints);

	Context->CompoundEdgesBlender = new PCGExDataBlending::FCompoundBlender(const_cast<FPCGExBlendingSettings*>(&Settings->EdgesBlendingSettings));
	Context->CompoundEdgesBlender->AddSources(*Context->MainEdges);

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
		PCGEX_DELETE(Context->CompoundGraph)
		PCGEX_DELETE(Context->GraphBuilder)
		PCGEX_DELETE(Context->PointEdgeIntersections)
		PCGEX_DELETE(Context->EdgeEdgeIntersections)

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges) { return false; }

			Context->CompoundGraph = new PCGExGraph::FCompoundGraph(Context->PointPointIntersectionSettings.FuseSettings, Context->CurrentIO->GetIn()->GetBounds().ExpandBy(10));
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
		Context->GetAsyncManager()->Start<PCGExGraphTask::FCompoundGraphInsertEdges>(
			Context->CurrentIO->IOIndex, Context->CurrentIO,
			Context->CompoundGraph, Context->CurrentEdges, &Context->NodeIndicesMap);

		Context->SetAsyncState(PCGExGraph::State_ProcessingEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
		Context->SetState(PCGExGraph::State_ReadyForNextEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingGraph))
	{
		const int32 NumCompoundNodes = Context->CompoundGraph->Nodes.Num();
		TArray<FPCGPoint>& MutablePoints = Context->CurrentIO->GetOut()->GetMutablePoints();

		auto Initialize = [&]() { MutablePoints.SetNum(NumCompoundNodes); };

		auto ProcessNode = [&](int32 Index)
		{
			PCGExGraph::FCompoundNode* CompoundNode = Context->CompoundGraph->Nodes[Index];
			MutablePoints[Index].Transform.SetLocation(CompoundNode->UpdateCenter(Context->CompoundGraph->PointsCompounds, Context->MainPoints));
		};

		if (!Context->Process(Initialize, ProcessNode, NumCompoundNodes)) { return false; }
		// Initiate merging

		Context->CompoundPointsBlender->Merge(
			Context->GetAsyncManager(), Context->CurrentIO,
			Context->CompoundGraph->PointsCompounds, PCGExSettings::GetDistanceSettings(Context->PointPointIntersectionSettings));

		Context->SetAsyncState(PCGExGraph::State_MergingPointCompounds);
	}

	//TODO : Merge edges, need to create a dummy PointIO for FGraphBuilder

	if (Context->IsState(PCGExGraph::State_MergingPointCompounds))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		Context->CompoundPointsBlender->Write();

		Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, &Context->GraphBuilderSettings, 6, Context->MainEdges);

		TArray<PCGExGraph::FUnsignedEdge> UniqueEdges;
		Context->CompoundGraph->GetUniqueEdges(UniqueEdges);
		Context->CompoundGraph->WriteMetadata(Context->GraphBuilder->Graph->NodeMetadata);

		Context->GraphBuilder->Graph->InsertEdges(UniqueEdges, -1);
		UniqueEdges.Empty();

		if (Settings->bDoPointEdgeIntersection)
		{
			Context->PointEdgeIntersections = new PCGExGraph::FPointEdgeIntersections(Context->GraphBuilder->Graph, Context->CompoundGraph, Context->CurrentIO, Context->PointEdgeIntersectionSettings);
			Context->PointEdgeIntersections->FindIntersections(Context);
			Context->SetAsyncState(PCGExGraph::State_FindingPointEdgeIntersections);
		}
		else if (Settings->bDoEdgeEdgeIntersection)
		{
			Context->EdgeEdgeIntersections = new PCGExGraph::FEdgeEdgeIntersections(Context->GraphBuilder->Graph, Context->CompoundGraph, Context->CurrentIO, Context->EdgeEdgeIntersectionSettings);
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
			Context->EdgeEdgeIntersections = new PCGExGraph::FEdgeEdgeIntersections(Context->GraphBuilder->Graph, Context->CompoundGraph, Context->CurrentIO, Context->EdgeEdgeIntersectionSettings);
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
