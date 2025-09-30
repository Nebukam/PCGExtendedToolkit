﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFuseClusters.h"

#include "Data/PCGExPointIO.h"
#include "Data/PCGExUnionData.h"
#include "Graph/PCGExIntersections.h"

#include "Data/Blending/PCGExUnionBlender.h"


#include "Graph/Data/PCGExClusterData.h"
#include "Graph/PCGExUnionProcessor.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EIOInit UPCGExFuseClustersSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::NoInit; }
PCGExData::EIOInit UPCGExFuseClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::NoInit; }

#pragma endregion

PCGEX_INITIALIZE_ELEMENT(FuseClusters)
PCGEX_ELEMENT_BATCH_EDGE_IMPL(FuseClusters)

bool FPCGExFuseClustersElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext))
	{
		return false;
	}

	PCGEX_CONTEXT_AND_SETTINGS(FuseClusters)

	PCGEX_FWD(VtxCarryOverDetails)
	Context->VtxCarryOverDetails.Init();

	PCGEX_FWD(EdgesCarryOverDetails)
	Context->EdgesCarryOverDetails.Init();

	const_cast<UPCGExFuseClustersSettings*>(Settings)->EdgeEdgeIntersectionDetails.Init();

	const TSharedPtr<PCGExData::FPointIO> UnionIO = PCGExData::NewPointIO(Context, PCGExGraph::OutputVerticesLabel);
	UnionIO->InitializeOutput<UPCGExClusterNodesData>(PCGExData::EIOInit::New);

	Context->UnionDataFacade = MakeShared<PCGExData::FFacade>(UnionIO.ToSharedRef());

	Context->UnionGraph = MakeShared<PCGExGraph::FUnionGraph>(
		Settings->PointPointIntersectionDetails.FuseDetails,
		Context->MainPoints->GetInBounds().ExpandBy(10));

	// TODO : Support local fuse distance, requires access to all input facades
	if (!Context->UnionGraph->Init(Context)) { return false; }

	Context->UnionGraph->EdgesUnion->bIsAbstract = false; // Because we have valid edge data

	Context->UnionProcessor = MakeShared<PCGExGraph::FUnionProcessor>(
		Context,
		Context->UnionDataFacade.ToSharedRef(),
		Context->UnionGraph.ToSharedRef(),
		Settings->PointPointIntersectionDetails,
		Settings->DefaultPointsBlendingDetails,
		Settings->DefaultEdgesBlendingDetails);

	Context->UnionProcessor->VtxCarryOverDetails = &Context->VtxCarryOverDetails;
	Context->UnionProcessor->EdgesCarryOverDetails = &Context->EdgesCarryOverDetails;

	if (Settings->bFindPointEdgeIntersections)
	{
		Context->UnionProcessor->InitPointEdge(
			Settings->PointEdgeIntersectionDetails,
			Settings->bUseCustomPointEdgeBlending,
			&Settings->CustomPointEdgeBlendingDetails);
	}

	if (Settings->bFindEdgeEdgeIntersections)
	{
		Context->UnionProcessor->InitEdgeEdge(
			Settings->EdgeEdgeIntersectionDetails,
			Settings->bUseCustomPointEdgeBlending,
			&Settings->CustomEdgeEdgeBlendingDetails);
	}

	return true;
}

bool FPCGExFuseClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFuseClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FuseClusters)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		const bool bDoInline = Settings->PointPointIntersectionDetails.FuseDetails.DoInlineInsertion();

		if (!Context->StartProcessingClusters(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
			{
				NewBatch->bSkipCompletion = true;
				NewBatch->bDaisyChainProcessing = bDoInline;
			}, bDoInline))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExGraph::State_PreparingUnion)

	PCGEX_ON_STATE(PCGExGraph::State_PreparingUnion)
	{
		const int32 NumFacades = Context->Batches.Num();

		Context->VtxFacades.Reserve(NumFacades);
		Context->UnionProcessor->SourceEdgesIO = &Context->EdgesDataFacades;

		for (const TSharedPtr<PCGExClusterMT::IBatch>& Batch : Context->Batches)
		{
			Context->VtxFacades.Add(Batch->VtxDataFacade);
		}

		if (!Context->UnionProcessor->StartExecution(Context->VtxFacades, Settings->GraphBuilderDetails)) { return true; }
	}

	if (!Context->UnionProcessor->Execute()) { return false; }

	(void)Context->UnionDataFacade->Source->StageOutput(Context);
	Context->Done();

	return Context->TryComplete();
}

namespace PCGExFuseClusters
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFuseClusters::Process);

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		VtxIOIndex = VtxDataFacade->Source->IOIndex;
		EdgesIOIndex = EdgeDataFacade->Source->IOIndex;

		// Prepare insertion
		Cluster = PCGExClusterData::TryGetCachedCluster(VtxDataFacade->Source, EdgeDataFacade->Source);

		if (!Cluster)
		{
			if (!BuildIndexedEdges(EdgeDataFacade->Source, *EndpointsLookup, IndexedEdges, true)) { return false; }
			if (IndexedEdges.IsEmpty()) { return false; }
		}
		else
		{
			NumNodes = Cluster->Nodes->Num();
			NumEdges = Cluster->Edges->Num();
		}

		bInvalidEdges = false;
		UnionGraph = Context->UnionGraph;

		bDaisyChainProcessEdges = Settings->PointPointIntersectionDetails.FuseDetails.DoInlineInsertion();

		const int32 NumIterations = Cluster ? Cluster->Edges->Num() : IndexedEdges.Num();

		if (bDaisyChainProcessEdges)
		{
			// Blunt insert since processor don't have a "wait"
			InsertEdges(PCGExMT::FScope(0, NumIterations), true);
			OnInsertionComplete();
		}
		else
		{
			PCGEX_ASYNC_GROUP_CHKD(AsyncManager, InsertEdges)

			InsertEdges->OnCompleteCallback = [PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->OnInsertionComplete();
			};

			InsertEdges->OnSubLoopStartCallback = [PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				This->InsertEdges(Scope, false);
			};

			InsertEdges->StartSubLoops(NumIterations, 256);
		}

		return true;
	}

	void FProcessor::InsertEdges(const PCGExMT::FScope& Scope, const bool bUnsafe)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFuseClusters::FProcessor::InsertEdges);

		if (Cluster)
		{
			if (bUnsafe)
			{
				PCGEX_SCOPE_LOOP(i)
				{
					const PCGExGraph::FEdge* Edge = Cluster->GetEdge(i);
					UnionGraph->InsertEdge_Unsafe(
						VtxDataFacade->GetInPoint(Edge->Start), VtxDataFacade->GetInPoint(Edge->End),
						EdgeDataFacade->GetInPoint(Edge->PointIndex));
				}
			}
			else
			{
				PCGEX_SCOPE_LOOP(i)
				{
					const PCGExGraph::FEdge* Edge = Cluster->GetEdge(i);
					UnionGraph->InsertEdge(
						VtxDataFacade->GetInPoint(Edge->Start), VtxDataFacade->GetInPoint(Edge->End),
						EdgeDataFacade->GetInPoint(Edge->PointIndex));
				}
			}
		}
		else
		{
			if (bUnsafe)
			{
				PCGEX_SCOPE_LOOP(i)
				{
					const PCGExGraph::FEdge& Edge = IndexedEdges[i];
					UnionGraph->InsertEdge_Unsafe(
						VtxDataFacade->GetInPoint(Edge.Start), VtxDataFacade->GetInPoint(Edge.End),
						EdgeDataFacade->GetInPoint(Edge.PointIndex));
				}
			}
			else
			{
				PCGEX_SCOPE_LOOP(i)
				{
					const PCGExGraph::FEdge& Edge = IndexedEdges[i];
					UnionGraph->InsertEdge(
						VtxDataFacade->GetInPoint(Edge.Start), VtxDataFacade->GetInPoint(Edge.End),
						EdgeDataFacade->GetInPoint(Edge.PointIndex));
				}
			}
		}
	}

	void FProcessor::OnInsertionComplete()
	{
	}
}

#undef LOCTEXT_NAMESPACE
