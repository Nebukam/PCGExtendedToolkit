// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFuseClusters.h"
#include "Graph/PCGExIntersections.h"

#include "Data/Blending/PCGExCompoundBlender.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "Graph/Data/PCGExClusterData.h"
#include "Graph/PCGExCompoundHelpers.h"

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

FPCGExFuseClustersContext::~FPCGExFuseClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE_TARRAY(VtxFacades)
	PCGEX_DELETE_FACADE_AND_SOURCE(CompoundFacade)

	PCGEX_DELETE(CompoundProcessor)
}

PCGEX_INITIALIZE_ELEMENT(FuseClusters)

bool FPCGExFuseClustersElement::Boot(FPCGContext* InContext) const
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

	const_cast<UPCGExFuseClustersSettings*>(Settings)
		->EdgeEdgeIntersectionDetails.ComputeDot();

	Context->CompoundProcessor = new PCGExGraph::FCompoundProcessor(
		Context,
		Settings->PointPointIntersectionDetails,
		Settings->DefaultPointsBlendingDetails,
		Settings->DefaultEdgesBlendingDetails);

	if (Settings->bFindPointEdgeIntersections)
	{
		Context->CompoundProcessor->InitPointEdge(
			Settings->PointEdgeIntersectionDetails,
			Settings->bUseCustomPointEdgeBlending,
			&Settings->CustomPointEdgeBlendingDetails);
	}

	if (Settings->bFindEdgeEdgeIntersections)
	{
		Context->CompoundProcessor->InitEdgeEdge(
			Settings->EdgeEdgeIntersectionDetails,
			Settings->bUseCustomPointEdgeBlending,
			&Settings->CustomEdgeEdgeBlendingDetails);
	}

	PCGExData::FPointIO* CompoundPoints = new PCGExData::FPointIO();
	CompoundPoints->SetInfos(-1, PCGExGraph::OutputVerticesLabel);
	CompoundPoints->InitializeOutput<UPCGExClusterNodesData>(
		PCGExData::EInit::NewOutput);

	Context->CompoundFacade = new PCGExData::FFacade(CompoundPoints);

	Context->CompoundGraph = new PCGExGraph::FCompoundGraph(
		Settings->PointPointIntersectionDetails.FuseDetails,
		Context->MainPoints->GetInBounds().ExpandBy(10),
		Settings->PointPointIntersectionDetails.FuseMethod);

	return true;
}

bool FPCGExFuseClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFuseClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FuseClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context))
		{
			return true;
		}

		const bool bDoInline = Settings->PointPointIntersectionDetails.DoInlineInsertion();

		if (!Context->StartProcessingClusters<
			PCGExClusterMT::TBatch<PCGExFuseClusters::FProcessor>>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExClusterMT::TBatch<PCGExFuseClusters::FProcessor>* NewBatch)
			{
				NewBatch->bInlineProcessing = bDoInline;
			},
			PCGExGraph::State_PreparingCompound, bDoInline))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	if (Context->IsState(PCGExGraph::State_PreparingCompound))
	{
		Context->VtxFacades.Reserve(Context->Batches.Num());
		for (PCGExClusterMT::FClusterProcessorBatchBase* Batch :
		     Context->Batches)
		{
			Context->VtxFacades.Add(Batch->VtxDataFacade);
			Batch->VtxDataFacade = nullptr; // Remove ownership of facade
			// before deleting the processor
		}

		PCGEX_DELETE_TARRAY(Context->Batches);

		if (!Context->CompoundProcessor->StartExecution(
			Context->CompoundGraph,
			Context->CompoundFacade,
			Context->VtxFacades,
			Settings->GraphBuilderDetails,
			&Settings->VtxCarryOverDetails)) { return true; }
	}

	if (!Context->CompoundProcessor->Execute()) { return false; }

	Context->CompoundFacade->Source->OutputTo(Context);
	Context->Done();

	return Context->TryComplete();
}

namespace PCGExFuseClusters
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFuseClusters::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FuseClusters)

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		VtxIOIndex = VtxIO->IOIndex;
		EdgesIOIndex = EdgesIO->IOIndex;

		// Prepare insertion
		bDeleteCluster = false;
		const PCGExCluster::FCluster* CachedCluster = PCGExClusterData::TryGetCachedCluster(VtxIO, EdgesIO);
		Cluster = CachedCluster ? const_cast<PCGExCluster::FCluster*>(CachedCluster) : nullptr;

		if (!Cluster)
		{
			if (!BuildIndexedEdges(EdgesIO, *EndpointsLookup, IndexedEdges, true)) { return false; }
			if (IndexedEdges.IsEmpty()) { return false; }
		}
		else
		{
			NumNodes = Cluster->Nodes->Num();
			NumEdges = Cluster->Edges->Num();
		}

		InPoints = &VtxIO->GetIn()->GetPoints();

		bInvalidEdges = false;
		CompoundGraph = TypedContext->CompoundGraph;

		bInlineProcessRange = bInlineProcessEdges = Settings->PointPointIntersectionDetails.DoInlineInsertion();

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
