// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFuseClusters.h"
#include "Graph/PCGExIntersections.h"

#include "Data/Blending/PCGExUnionBlender.h"


#include "Graph/Data/PCGExClusterData.h"
#include "Graph/PCGExUnionHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExFuseClustersSettings::GetMainOutputInitMode() const
{
	return PCGExData::EInit::NoOutput;
}

PCGExData::EInit UPCGExFuseClustersSettings::GetEdgeOutputInitMode() const
{
	return PCGExData::EInit::NoOutput;
}

#pragma endregion

PCGEX_INITIALIZE_ELEMENT(FuseClusters)

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

	const TSharedPtr<PCGExData::FPointIO> UnionIO = MakeShared<PCGExData::FPointIO>(Context);
	UnionIO->SetInfos(-1, PCGExGraph::OutputVerticesLabel);
	UnionIO->InitializeOutput<UPCGExClusterNodesData>(Context, PCGExData::EInit::NewOutput);

	Context->UnionDataFacade = MakeShared<PCGExData::FFacade>(UnionIO.ToSharedRef());

	Context->UnionGraph = MakeShared<PCGExGraph::FUnionGraph>(
		Settings->PointPointIntersectionDetails.FuseDetails,
		Context->MainPoints->GetInBounds().ExpandBy(10));

	Context->UnionGraph->EdgesUnion->bIsAbstract = false; // Because we have valid edge data

	Context->UnionProcessor = MakeShared<PCGExGraph::FUnionProcessor>(
		Context,
		Context->UnionDataFacade.ToSharedRef(),
		Context->UnionGraph.ToSharedRef(),
		Settings->PointPointIntersectionDetails,
		Settings->DefaultPointsBlendingDetails,
		Settings->DefaultEdgesBlendingDetails);

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

		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatch<PCGExFuseClusters::FProcessor>>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::TBatch<PCGExFuseClusters::FProcessor>>& NewBatch)
			{
				NewBatch->bInlineProcessing = bDoInline;
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

		for (const TSharedPtr<PCGExClusterMT::FClusterProcessorBatchBase>& Batch : Context->Batches)
		{
			Context->VtxFacades.Add(Batch->VtxDataFacade);
		}

		if (!Context->UnionProcessor->StartExecution(
			Context->VtxFacades,
			Settings->GraphBuilderDetails,
			&Settings->VtxCarryOverDetails)) { return true; }
	}

	if (!Context->UnionProcessor->Execute()) { return false; }

	Context->UnionDataFacade->Source->StageOutput();

	Context->Done();

	return Context->TryComplete();
}

namespace PCGExFuseClusters
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFuseClusters::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

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

		InPoints = &VtxDataFacade->Source->GetIn()->GetPoints();

		bInvalidEdges = false;
		UnionGraph = Context->UnionGraph;

		bInlineProcessRange = bInlineProcessEdges = Settings->PointPointIntersectionDetails.FuseDetails.DoInlineInsertion();

		if (Cluster) { StartParallelLoopForEdges(); }
		else { StartParallelLoopForRange(IndexedEdges.Num()); }

		return true;
	}

	void FProcessor::CompleteWork()
	{
		if (bInvalidEdges)
		{
		}
	}
}

#undef LOCTEXT_NAMESPACE
