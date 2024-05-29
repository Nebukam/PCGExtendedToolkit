// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFuseClusters.h"
#include "Graph/PCGExIntersections.h"

#include "Data/PCGExGraphDefinition.h"
#include "Data/Blending/PCGExCompoundBlender.h"
#include "Data/Blending/PCGExMetadataBlender.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExFuseClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }
PCGExData::EInit UPCGExFuseClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

#pragma endregion

FPCGExFuseClustersContext::~FPCGExFuseClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(CompoundGraph)
	PCGEX_DELETE(GraphBuilder)
	PCGEX_DELETE(PointEdgeIntersections)
	PCGEX_DELETE(EdgeEdgeIntersections)

	PCGEX_DELETE(CompoundPointsBlender)
	PCGEX_DELETE(CompoundEdgesBlender)
	PCGEX_DELETE(MetadataBlender)
}

PCGEX_INITIALIZE_ELEMENT(FuseClusters)

bool FPCGExFuseClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FuseClusters)

	const_cast<UPCGExFuseClustersSettings*>(Settings)->EdgeEdgeIntersectionSettings.ComputeDot();

	Context->GraphMetadataSettings.Grab(Context, Settings->PointPointIntersectionSettings);
	Context->GraphMetadataSettings.Grab(Context, Settings->PointEdgeIntersectionSettings);
	Context->GraphMetadataSettings.Grab(Context, Settings->EdgeEdgeIntersectionSettings);

	PCGEX_FWD(GraphBuilderSettings)

	Context->CompoundGraph = new PCGExGraph::FCompoundGraph(Settings->PointPointIntersectionSettings.FuseSettings, Context->MainPoints->GetInBounds().ExpandBy(10));

	Context->CompoundPointsBlender = new PCGExDataBlending::FCompoundBlender(const_cast<FPCGExBlendingSettings*>(&Settings->DefaultPointsBlendingSettings));
	Context->CompoundPointsBlender->AddSources(*Context->MainPoints);

	Context->CompoundEdgesBlender = new PCGExDataBlending::FCompoundBlender(const_cast<FPCGExBlendingSettings*>(&Settings->DefaultEdgesBlendingSettings));
	Context->CompoundEdgesBlender->AddSources(*Context->MainEdges);

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
		if (Context->CurrentEdges) { Context->CurrentEdges->CleanupKeys(); } // Careful as this may impair data blending later on?
		if (!Context->AdvanceEdges(false))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		Context->CurrentEdges->CreateInKeys();

		// Insert current edges into loose graph
		// Note that since we're building from edges only, this skips isolated points altogether

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
		// Create consolidated nodes from compound graph
		const int32 NumCompoundNodes = Context->CompoundGraph->Nodes.Num();

		if (NumCompoundNodes == 0)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Compound graph is empty. Vtx/Edge pairs are probably corrupted."));
			Context->Done();
			return false;
		}

		TArray<FPCGPoint>& MutablePoints = Context->ConsolidatedPoints->GetOut()->GetMutablePoints();

		auto Initialize = [&]() { Context->ConsolidatedPoints->SetNumInitialized(NumCompoundNodes, true); };

		auto ProcessNode = [&](const int32 Index)
		{
			PCGExGraph::FCompoundNode* CompoundNode = Context->CompoundGraph->Nodes[Index];
			MutablePoints[Index].Transform.SetLocation(CompoundNode->UpdateCenter(Context->CompoundGraph->PointsCompounds, Context->MainPoints));
		};

		if (!Context->Process(Initialize, ProcessNode, NumCompoundNodes)) { return false; }

		// Initiate merging
		Context->CompoundPointsBlender->PrepareMerge(Context->ConsolidatedPoints, Context->CompoundGraph->PointsCompounds);
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

		Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->ConsolidatedPoints, &Context->GraphBuilderSettings, 6, Context->MainEdges);

		TArray<PCGExGraph::FUnsignedEdge> UniqueEdges;
		Context->CompoundGraph->GetUniqueEdges(UniqueEdges);
		Context->CompoundGraph->WriteMetadata(Context->GraphBuilder->Graph->NodeMetadata);

		Context->GraphBuilder->Graph->InsertEdges(UniqueEdges, -1);
		UniqueEdges.Empty();

		if (Settings->bFindPointEdgeIntersections)
		{
			Context->PointEdgeIntersections = new PCGExGraph::FPointEdgeIntersections(Context->GraphBuilder->Graph, Context->CompoundGraph, Context->ConsolidatedPoints, Settings->PointEdgeIntersectionSettings);
			Context->SetState(PCGExGraph::State_FindingPointEdgeIntersections);
		}
		else if (Settings->bFindEdgeEdgeIntersections)
		{
			Context->EdgeEdgeIntersections = new PCGExGraph::FEdgeEdgeIntersections(Context->GraphBuilder->Graph, Context->CompoundGraph, Context->ConsolidatedPoints, Settings->EdgeEdgeIntersectionSettings);
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
			FindCollinearNodes(Context->PointEdgeIntersections, EdgeIndex, Context->ConsolidatedPoints->GetOut());
		};

		if (!Context->Process(PointEdge, Context->GraphBuilder->Graph->Edges.Num())) { return false; }

		Context->PointEdgeIntersections->Insert();  // TODO : Async?
		Context->ConsolidatedPoints->CleanupKeys(); // Required for later blending as point count has changed

		Context->SetState(PCGExGraph::State_BlendingPointEdgeCrossings);
	}

	if (Context->IsState(PCGExGraph::State_BlendingPointEdgeCrossings))
	{
		auto Initialize = [&]()
		{
			if (Settings->bUseCustomPointEdgeBlending) { Context->MetadataBlender = new PCGExDataBlending::FMetadataBlender(const_cast<FPCGExBlendingSettings*>(&Settings->CustomPointEdgeBlendingSettings)); }
			else { Context->MetadataBlender = new PCGExDataBlending::FMetadataBlender(const_cast<FPCGExBlendingSettings*>(&Settings->DefaultPointsBlendingSettings)); }

			Context->MetadataBlender->PrepareForData(*Context->ConsolidatedPoints, PCGExData::ESource::Out);
			Context->MetadataBlender->InitializeFromScratch();
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
			Context->EdgeEdgeIntersections = new PCGExGraph::FEdgeEdgeIntersections(Context->GraphBuilder->Graph, Context->CompoundGraph, Context->ConsolidatedPoints, Settings->EdgeEdgeIntersectionSettings);
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

		Context->EdgeEdgeIntersections->Insert();   // TODO : Async?
		Context->ConsolidatedPoints->CleanupKeys(); // Required for later blending as point count has changed

		Context->SetState(PCGExGraph::State_BlendingEdgeEdgeCrossings);
	}

	if (Context->IsState(PCGExGraph::State_BlendingEdgeEdgeCrossings))
	{
		auto Initialize = [&]()
		{
			if (Settings->bUseCustomPointEdgeBlending) { Context->MetadataBlender = new PCGExDataBlending::FMetadataBlender(const_cast<FPCGExBlendingSettings*>(&Settings->CustomEdgeEdgeBlendingSettings)); }
			else { Context->MetadataBlender = new PCGExDataBlending::FMetadataBlender(const_cast<FPCGExBlendingSettings*>(&Settings->DefaultPointsBlendingSettings)); }

			Context->MetadataBlender->PrepareForData(*Context->ConsolidatedPoints, PCGExData::ESource::Out);
			Context->MetadataBlender->InitializeFromScratch();
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

			Context->GraphBuilder->PointIO->Flatten();
			Context->GraphBuilder->Write(Context);
			Context->OutputPoints();
		}

		Context->Done();
	}

	if (Context->IsState(PCGExGraph::State_MergingEdgeCompounds))
	{
		//TODO	
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
