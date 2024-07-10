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

	if (CompoundFacade)
	{
		PCGEX_DELETE(CompoundFacade->Source)
		PCGEX_DELETE(CompoundFacade)
	}

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

	PCGExData::FPointIO* CompoundPoints = new PCGExData::FPointIO(nullptr);
	CompoundPoints->SetInfos(-1, PCGExGraph::OutputVerticesLabel);
	CompoundPoints->InitializeOutput<UPCGExClusterNodesData>(
		PCGExData::EInit::NewOutput);

	Context->CompoundFacade = new PCGExData::FFacade(CompoundPoints);

	Context->CompoundGraph = new PCGExGraph::FCompoundGraph(
		Settings->PointPointIntersectionDetails.FuseDetails,
		Context->MainPoints->GetInBounds().ExpandBy(10),
		true,
		Settings->PointPointIntersectionDetails.FuseMethod);

	Context->CompoundPointsBlender = new PCGExDataBlending::FCompoundBlender(
		&Settings->DefaultPointsBlendingDetails, &Context->VtxCarryOverDetails);

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

		if (!Context->StartProcessingClusters<
			PCGExClusterMT::TBatch<PCGExFuseClusters::FProcessor>>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExClusterMT::TBatch<PCGExFuseClusters::FProcessor>* NewBatch)
			{
				NewBatch->bInlineProcessing = true; //Context->CompoundGraph->Octree ? true : false;
			},
			PCGExGraph::State_ProcessingCompound,
			true)) //Context->CompoundGraph->Octree ? true : false))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	if (Context->IsState(PCGExGraph::State_ProcessingCompound))
	{
		const int32 NumCompoundNodes = Context->CompoundGraph->Nodes.Num();
		if (NumCompoundNodes == 0)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Compound graph is empty. Vtx/Edge pairs are likely corrupted."));
			return true;
		}

		TArray<FPCGPoint>& MutablePoints =
			Context->CompoundFacade->GetOut()->GetMutablePoints();

		auto Initialize = [&]()
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
			Context->CompoundFacade->Source->InitializeNum(
				NumCompoundNodes, true);
			Context->CompoundPointsBlender->AddSources(Context->VtxFacades);
			Context->CompoundPointsBlender->PrepareMerge(
				Context->CompoundFacade, Context->CompoundGraph->PointsCompounds);
		};

		auto ProcessNode = [&](const int32 Index)
		{
			PCGExGraph::FCompoundNode* CompoundNode =
				Context->CompoundGraph->Nodes[Index];
			PCGMetadataEntryKey Key = MutablePoints[Index].MetadataEntry;
			MutablePoints[Index] =
				CompoundNode->Point; // Copy "original" point properties, in case
			// there's only one

			FPCGPoint& Point = MutablePoints[Index];
			Point.MetadataEntry = Key; // Restore key

			Point.Transform.SetLocation(
				CompoundNode->UpdateCenter(
					Context->CompoundGraph->PointsCompounds, Context->MainPoints));
			Context->CompoundPointsBlender->MergeSingle(
				Index,
				PCGExDetails::GetDistanceDetails(
					Settings->PointPointIntersectionDetails));
		};

		if (!Context->Process(Initialize, ProcessNode, NumCompoundNodes))
		{
			return false;
		}

		PCGEX_DELETE(Context->CompoundPointsBlender)

		Context->CompoundFacade->Write(Context->GetAsyncManager(), true);
		Context->SetAsyncState(PCGExMT::State_CompoundWriting);
	}

	if (Context->IsState(PCGExMT::State_CompoundWriting))
	{
		PCGEX_ASYNC_WAIT

		Context->CompoundProcessor->StartProcessing(
			Context->CompoundGraph,
			Context->CompoundFacade,
			Settings->GraphBuilderDetails,
			[&](PCGExGraph::FGraphBuilder* GraphBuilder)
			{
				TArray<PCGExGraph::FUnsignedEdge> UniqueEdges;
				Context->CompoundGraph->GetUniqueEdges(UniqueEdges);
				Context->CompoundGraph->WriteMetadata(
					GraphBuilder->Graph->NodeMetadata);

				GraphBuilder->Graph->InsertEdges(UniqueEdges, -1);
			});
	}

	if (!Context->CompoundProcessor->Execute())
	{
		return false;
	}

	Context->CompoundFacade->Source->OutputTo(Context);
	Context->Done();

	return Context->TryComplete();
}

namespace PCGExFuseClusters
{
	FProcessor::~FProcessor()
	{
	}

	bool
	FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FuseClusters)

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

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

		bInlineProcessRange = bInlineProcessEdges = true; // TypedContext->CompoundGraph->Octree ? true : false;

		if (Cluster) { StartParallelLoopForEdges(); }
		else { StartParallelLoopForRange(IndexedEdges.Num()); }

		return true;
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFusePointsElement::ProcessSingleRangeIteration);

		const PCGExGraph::FIndexedEdge& Edge = IndexedEdges[Iteration];

		CompoundGraph->InsertEdge(
			(*InPoints)[Edge.Start], VtxIO->IOIndex, Edge.Start,
			(*InPoints)[Edge.End], VtxIO->IOIndex, Edge.End,
			EdgesIO->IOIndex, Edge.PointIndex);
	}

	void FProcessor::ProcessSingleEdge(PCGExGraph::FIndexedEdge& Edge)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFusePointsElement::ProcessSingleEdge);

		CompoundGraph->InsertEdge(
			(*InPoints)[Edge.Start], VtxIO->IOIndex, Edge.Start,
			(*InPoints)[Edge.End], VtxIO->IOIndex, Edge.End,
			EdgesIO->IOIndex, Edge.PointIndex);
	}

	void FProcessor::CompleteWork()
	{
		if (bInvalidEdges)
		{
		}

		/*
		                const TArray<FPCGPoint>& InPoints =
		   VtxIO->GetIn()->GetPoints();
	
		                const int32 VtxIOIndex = VtxIO->IOIndex;
		                const int32 EdgesIOIndex = EdgesIO->IOIndex;
	
		                for (const PCGExGraph::FIndexedEdge& Edge : IndexedEdges)
		                {
		                        TypedContext->CompoundGraph->CreateBridge(
		                                InPoints[Edge.Start], VtxIOIndex,
		   Edge.Start, InPoints[Edge.End], VtxIOIndex, Edge.End, EdgesIOIndex,
		   Edge.PointIndex);
		                }
		*/
	}
}

#undef LOCTEXT_NAMESPACE
