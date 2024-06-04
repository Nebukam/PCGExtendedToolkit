// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFuseClustersLocal.h"

#include "Data/Blending/PCGExCompoundBlender.h"
#include "Data/PCGExGraphDefinition.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "Graph/PCGExIntersections.h"

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
	PCGEX_DELETE(MetadataBlender)
}

PCGEX_INITIALIZE_ELEMENT(FuseClustersLocal)

bool FPCGExFuseClustersLocalElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FuseClustersLocal)

	const_cast<UPCGExFuseClustersLocalSettings*>(Settings)->EdgeEdgeIntersectionSettings.ComputeDot();

	Context->GraphMetadataSettings.Grab(Context, Settings->PointPointIntersectionSettings);
	Context->GraphMetadataSettings.Grab(Context, Settings->PointEdgeIntersectionSettings);
	Context->GraphMetadataSettings.Grab(Context, Settings->EdgeEdgeIntersectionSettings);

	//Context->PointEdgeIntersectionSettings.MakeSafeForTolerance(Context->PointPointSettings.FuseSettings.Tolerance);
	//Context->EdgeEdgeIntersectionSettings.MakeSafeForTolerance(Context->PointEdgeIntersectionSettings.FuseSettings.Tolerance);
	//Context->EdgeEdgeIntersectionSettings.Init();

	PCGEX_FWD(GraphBuilderSettings)

	Context->CompoundPointsBlender = new PCGExDataBlending::FCompoundBlender(const_cast<FPCGExBlendingSettings*>(&Settings->DefaultPointsBlendingSettings));
	Context->CompoundPointsBlender->AddSources(*Context->MainPoints);

	Context->CompoundEdgesBlender = new PCGExDataBlending::FCompoundBlender(const_cast<FPCGExBlendingSettings*>(&Settings->DefaultEdgesBlendingSettings));
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

			Context->CompoundGraph = new PCGExGraph::FCompoundGraph(Settings->PointPointIntersectionSettings.FuseSettings, Context->CurrentIO->GetIn()->GetBounds().ExpandBy(10));
			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		if (Context->CurrentEdges) { Context->CurrentEdges->CleanupKeys(); }
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
		PCGEX_WAIT_ASYNC
		Context->SetState(PCGExGraph::State_ReadyForNextEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingGraph))
	{
		const int32 NumCompoundNodes = Context->CompoundGraph->Nodes.Num();

		if (NumCompoundNodes == 0)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Compound graph is empty at least one Vtx/Edge pair. They are probably corrupted."));
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		TArray<FPCGPoint>& MutablePoints = Context->CurrentIO->GetOut()->GetMutablePoints();

		auto Initialize = [&]() { Context->CurrentIO->SetNumInitialized(NumCompoundNodes, true); };

		auto ProcessNode = [&](const int32 Index)
		{
			PCGExGraph::FCompoundNode* CompoundNode = Context->CompoundGraph->Nodes[Index];
			MutablePoints[Index].Transform.SetLocation(CompoundNode->UpdateCenter(Context->CompoundGraph->PointsCompounds, Context->MainPoints));
		};

		if (!Context->Process(Initialize, ProcessNode, NumCompoundNodes)) { return false; }

		// Initiate merging
		Context->CompoundPointsBlender->PrepareMerge(Context->CurrentIO, Context->CompoundGraph->PointsCompounds);
		Context->SetState(PCGExGraph::State_MergingPointCompounds);
	}

	if (Context->IsState(PCGExGraph::State_MergingPointCompounds))
	{
		auto MergeCompound = [&](const int32 CompoundIndex)
		{
			Context->CompoundPointsBlender->MergeSingle(CompoundIndex, PCGExSettings::GetDistanceSettings(Settings->PointPointIntersectionSettings));
		};

		if (!Context->Process(MergeCompound, Context->CompoundGraph->NumNodes())) { return false; }

		Context->CompoundPointsBlender->Write();

		Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, &Context->GraphBuilderSettings, 6, Context->MainEdges);

		TArray<PCGExGraph::FUnsignedEdge> UniqueEdges;
		Context->CompoundGraph->GetUniqueEdges(UniqueEdges);
		Context->CompoundGraph->WriteMetadata(Context->GraphBuilder->Graph->NodeMetadata);

		Context->GraphBuilder->Graph->InsertEdges(UniqueEdges, -1);
		UniqueEdges.Empty();

		if (Settings->bFindPointEdgeIntersections)
		{
			Context->PointEdgeIntersections = new PCGExGraph::FPointEdgeIntersections(Context->GraphBuilder->Graph, Context->CompoundGraph, Context->CurrentIO, Settings->PointEdgeIntersectionSettings);
			Context->SetState(PCGExGraph::State_FindingPointEdgeIntersections);
		}
		else if (Settings->bFindEdgeEdgeIntersections)
		{
			Context->EdgeEdgeIntersections = new PCGExGraph::FEdgeEdgeIntersections(Context->GraphBuilder->Graph, Context->CompoundGraph, Context->CurrentIO, Settings->EdgeEdgeIntersectionSettings);
			Context->SetState(PCGExGraph::State_FindingEdgeEdgeIntersections);
		}
		else
		{
			Context->SetAsyncState(PCGExGraph::State_WritingClusters);
		}
	}

	if (Context->IsState(PCGExGraph::State_FindingPointEdgeIntersections))
	{
		auto PointEdge = [&](const int32 EdgeIndex)
		{
			const PCGExGraph::FIndexedEdge& Edge = Context->GraphBuilder->Graph->Edges[EdgeIndex];
			if (!Edge.bValid) { return; }
			FindCollinearNodes(Context->PointEdgeIntersections, EdgeIndex, Context->CurrentIO->GetOut());
		};

		if (!Context->Process(PointEdge, Context->GraphBuilder->Graph->Edges.Num())) { return false; }

		Context->PointEdgeIntersections->Insert(); // TODO : Async?
		Context->CurrentIO->CleanupKeys();         // Required for later blending as point count has changed

		Context->SetState(PCGExGraph::State_BlendingPointEdgeCrossings);
	}

	if (Context->IsState(PCGExGraph::State_BlendingPointEdgeCrossings))
	{
		auto Initialize = [&]()
		{
			if (Settings->bUseCustomPointEdgeBlending) { Context->MetadataBlender = new PCGExDataBlending::FMetadataBlender(const_cast<FPCGExBlendingSettings*>(&Settings->CustomPointEdgeBlendingSettings)); }
			else { Context->MetadataBlender = new PCGExDataBlending::FMetadataBlender(const_cast<FPCGExBlendingSettings*>(&Settings->DefaultPointsBlendingSettings)); }

			Context->MetadataBlender->PrepareForData(*Context->CurrentIO, PCGExData::ESource::Out, true);
		};

		auto BlendPointEdgeMetadata = [&](const int32 Index)
		{
		};

		if (!Context->Process(Initialize, BlendPointEdgeMetadata, Context->PointEdgeIntersections->Edges.Num())) { return false; }

		if (Context->MetadataBlender) { Context->MetadataBlender->Write(); }

		PCGEX_DELETE(Context->PointEdgeIntersections)
		PCGEX_DELETE(Context->MetadataBlender)

		if (Settings->bFindEdgeEdgeIntersections)
		{
			Context->EdgeEdgeIntersections = new PCGExGraph::FEdgeEdgeIntersections(Context->GraphBuilder->Graph, Context->CompoundGraph, Context->CurrentIO, Settings->EdgeEdgeIntersectionSettings);
			Context->SetState(PCGExGraph::State_FindingEdgeEdgeIntersections);
		}
		else
		{
			Context->SetAsyncState(PCGExGraph::State_WritingClusters);
		}
	}

	if (Context->IsState(PCGExGraph::State_FindingEdgeEdgeIntersections))
	{
		auto EdgeEdge = [&](const int32 EdgeIndex)
		{
			const PCGExGraph::FIndexedEdge& Edge = Context->GraphBuilder->Graph->Edges[EdgeIndex];
			if (!Edge.bValid) { return; }
			FindOverlappingEdges(Context->EdgeEdgeIntersections, EdgeIndex);
		};

		if (!Context->Process(EdgeEdge, Context->GraphBuilder->Graph->Edges.Num())) { return false; }

		Context->EdgeEdgeIntersections->Insert(); // TODO : Async?
		Context->CurrentIO->CleanupKeys();        // Required for later blending as point count has changed

		Context->SetState(PCGExGraph::State_BlendingEdgeEdgeCrossings);
	}

	if (Context->IsState(PCGExGraph::State_BlendingEdgeEdgeCrossings))
	{
		auto Initialize = [&]()
		{
			if (Settings->bUseCustomEdgeEdgeBlending) { Context->MetadataBlender = new PCGExDataBlending::FMetadataBlender(const_cast<FPCGExBlendingSettings*>(&Settings->CustomEdgeEdgeBlendingSettings)); }
			else { Context->MetadataBlender = new PCGExDataBlending::FMetadataBlender(const_cast<FPCGExBlendingSettings*>(&Settings->DefaultPointsBlendingSettings)); }

			Context->MetadataBlender->PrepareForData(*Context->CurrentIO, PCGExData::ESource::Out, true);
		};

		auto BlendCrossingMetadata = [&](const int32 Index) { Context->EdgeEdgeIntersections->BlendIntersection(Index, Context->MetadataBlender); };

		if (!Context->Process(Initialize, BlendCrossingMetadata, Context->EdgeEdgeIntersections->Crossings.Num())) { return false; }

		if (Context->MetadataBlender) { Context->MetadataBlender->Write(); }

		PCGEX_DELETE(Context->EdgeEdgeIntersections)
		PCGEX_DELETE(Context->MetadataBlender)

		Context->SetAsyncState(PCGExGraph::State_WritingClusters);
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		PCGEX_WAIT_ASYNC

		Context->GraphBuilder->Compile(Context, &Context->GraphMetadataSettings);
		Context->SetAsyncState(PCGExGraph::State_WaitingOnWritingClusters);
		return false;
	}

	if (Context->IsState(PCGExGraph::State_WaitingOnWritingClusters))
	{
		PCGEX_WAIT_ASYNC
		if (Context->GraphBuilder->bCompiledSuccessfully)
		{
			//TODO : Need to merge edge compounds once we have the final edge configuration.

			Context->GraphBuilder->Write(Context);
		}

		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExGraph::State_MergingEdgeCompounds))
	{
		//TODO	
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
