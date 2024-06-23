// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFuseClusters.h"
#include "Graph/PCGExIntersections.h"

#include "Data/PCGExGraphDefinition.h"
#include "Data/Blending/PCGExCompoundBlender.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "Graph/PCGExClusterData.h"
#include "Graph/PCGExCompoundHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExFuseClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }
PCGExData::EInit UPCGExFuseClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

#pragma endregion

FPCGExFuseClustersContext::~FPCGExFuseClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(CompoundPoints)

	PCGEX_DELETE(CompoundProcessor)
}

PCGEX_INITIALIZE_ELEMENT(FuseClusters)

bool FPCGExFuseClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FuseClusters)

	const_cast<UPCGExFuseClustersSettings*>(Settings)->EdgeEdgeIntersectionSettings.ComputeDot();

	Context->CompoundProcessor = new PCGExGraph::FCompoundProcessor(
		Context,
		Settings->PointPointIntersectionSettings,
		Settings->DefaultPointsBlendingSettings,
		Settings->DefaultEdgesBlendingSettings);

	if (Settings->bFindPointEdgeIntersections)
	{
		Context->CompoundProcessor->InitPointEdge(
			Settings->PointEdgeIntersectionSettings,
			Settings->bUseCustomPointEdgeBlending,
			&Settings->CustomPointEdgeBlendingSettings);
	}

	if (Settings->bFindEdgeEdgeIntersections)
	{
		Context->CompoundProcessor->InitEdgeEdge(
			Settings->EdgeEdgeIntersectionSettings,
			Settings->bUseCustomPointEdgeBlending,
			&Settings->CustomEdgeEdgeBlendingSettings);
	}

	Context->CompoundPoints = new PCGExData::FPointIO(nullptr);
	Context->CompoundPoints->SetInfos(-1, PCGExGraph::OutputVerticesLabel);
	Context->CompoundPoints->InitializeOutput<UPCGExClusterNodesData>(PCGExData::EInit::NewOutput);
	
	Context->CompoundGraph = new PCGExGraph::FCompoundGraph(
			Settings->PointPointIntersectionSettings.FuseSettings,
			Context->MainPoints->GetInBounds().ExpandBy(10), true,
			Settings->PointPointIntersectionSettings.Precision == EPCGExFusePrecision::Fast
		);

	Context->CompoundPointsBlender = new PCGExDataBlending::FCompoundBlender(&Settings->DefaultPointsBlendingSettings);
	Context->CompoundPointsBlender->AddSources(*Context->MainPoints);

	return true;
}

bool FPCGExFuseClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFuseClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FuseClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		// Sort edges & insert them

		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatch<PCGExFuseClusters::FProcessor>>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExClusterMT::TBatch<PCGExFuseClusters::FProcessor>* NewBatch)
			{
				NewBatch->bInlineProcessing = !Context->CompoundGraph->bFastMode;
			},
			PCGExMT::State_ProcessingPoints, !Context->CompoundGraph->bFastMode))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		const int32 NumCompoundNodes = Context->CompoundGraph->Nodes.Num();
		if (NumCompoundNodes == 0)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Compound graph is empty. Vtx/Edge pairs are likely corrupted."));
			return true;
		}

		TArray<FPCGPoint>& MutablePoints = Context->CompoundPoints->GetOut()->GetMutablePoints();

		auto Initialize = [&]()
		{
			PCGEX_DELETE_TARRAY(Context->Batches); // Cleanup processing data
			Context->CompoundPoints->SetNumInitialized(NumCompoundNodes, true);
			Context->CompoundPointsBlender->PrepareMerge(Context->CompoundPoints, Context->CompoundGraph->PointsCompounds);
		};

		auto ProcessNode = [&](const int32 Index)
		{
			PCGExGraph::FCompoundNode* CompoundNode = Context->CompoundGraph->Nodes[Index];
			PCGMetadataEntryKey Key = MutablePoints[Index].MetadataEntry;
			MutablePoints[Index] = CompoundNode->Point; // Copy "original" point properties, in case there's only one

			FPCGPoint& Point = MutablePoints[Index];
			Point.MetadataEntry = Key; // Restore key

			Point.Transform.SetLocation(CompoundNode->UpdateCenter(Context->CompoundGraph->PointsCompounds, Context->MainPoints));
			Context->CompoundPointsBlender->MergeSingle(Index, PCGExSettings::GetDistanceSettings(Settings->PointPointIntersectionSettings));
		};

		if (!Context->Process(Initialize, ProcessNode, NumCompoundNodes)) { return false; }

		Context->CompoundPointsBlender->Write(Context->GetAsyncManager());
		Context->SetAsyncState(PCGExMT::State_CompoundWriting);
	}

	if (Context->IsState(PCGExMT::State_CompoundWriting))
	{
		PCGEX_ASYNC_WAIT

		PCGEX_DELETE(Context->CompoundPointsBlender)

		Context->CompoundProcessor->StartProcessing(
			Context->CompoundGraph,
			Context->CompoundPoints,
			Settings->GraphBuilderSettings,
			[&](PCGExGraph::FGraphBuilder* GraphBuilder)
			{
				TArray<PCGExGraph::FUnsignedEdge> UniqueEdges;
				Context->CompoundGraph->GetUniqueEdges(UniqueEdges);
				Context->CompoundGraph->WriteMetadata(GraphBuilder->Graph->NodeMetadata);

				GraphBuilder->Graph->InsertEdges(UniqueEdges, -1);
			});
	}

	if (!Context->CompoundProcessor->Execute()) { return false; }

	Context->Done();
	Context->CompoundPoints->OutputTo(Context);
	Context->ExecuteEnd();

	return Context->IsDone();
}

namespace PCGExFuseClusters
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FuseClusters)

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		// Prepare insertion

		if (!BuildIndexedEdges(EdgesIO, *EndpointsLookup, IndexedEdges, true)) { return false; }
		if (IndexedEdges.IsEmpty()) { return false; }

		bInvalidEdges = false;

		const TArray<FPCGPoint>& InPoints = VtxIO->GetIn()->GetPoints();
		StartParallelLoopForRange(IndexedEdges.Num());
		return true;
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FuseClusters)

		const PCGExGraph::FIndexedEdge& Edge = IndexedEdges[Iteration];

		const TArray<FPCGPoint>& InPoints = VtxIO->GetIn()->GetPoints();

		const int32 VtxIOIndex = VtxIO->IOIndex;
		const int32 EdgesIOIndex = EdgesIO->IOIndex;

		TypedContext->CompoundGraph->CreateBridge(
			InPoints[Edge.Start], VtxIOIndex, Edge.Start,
			InPoints[Edge.End], VtxIOIndex, Edge.End, EdgesIOIndex, Edge.PointIndex);
	}

	void FProcessor::CompleteWork()
	{
		if (bInvalidEdges) { return; }

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FuseClusters)
		/*
				const TArray<FPCGPoint>& InPoints = VtxIO->GetIn()->GetPoints();
		
				const int32 VtxIOIndex = VtxIO->IOIndex;
				const int32 EdgesIOIndex = EdgesIO->IOIndex;
		
				for (const PCGExGraph::FIndexedEdge& Edge : IndexedEdges)
				{
					TypedContext->CompoundGraph->CreateBridge(
						InPoints[Edge.Start], VtxIOIndex, Edge.Start,
						InPoints[Edge.End], VtxIOIndex, Edge.End, EdgesIOIndex, Edge.PointIndex);
				}
		*/
	}
}

#undef LOCTEXT_NAMESPACE
