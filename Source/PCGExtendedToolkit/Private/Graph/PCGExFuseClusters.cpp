// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFuseClusters.h"
#include "Graph/PCGExIntersections.h"

#include "Data/PCGExGraphDefinition.h"
#include "Data/Blending/PCGExCompoundBlender.h"
#include "Data/Blending/PCGExMetadataBlender.h"
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

	Context->CompoundPoints = new PCGExData::FPointIO(PCGExGraph::OutputVerticesLabel, PCGExData::EInit::NewOutput);
	Context->CompoundGraph = new PCGExGraph::FCompoundGraph(Settings->PointPointIntersectionSettings.FuseSettings, Context->MainPoints->GetInBounds().ExpandBy(10));

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

		if (!Context->StartProcessingClusters<PCGExFuseClusters::FFuseBatch>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExFuseClusters::FFuseBatch* NewBatch)
			{
			},
			PCGExMT::State_ProcessingPoints, true))
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
		};

		auto ProcessNode = [&](const int32 Index)
		{
			PCGExGraph::FCompoundNode* CompoundNode = Context->CompoundGraph->Nodes[Index];
			MutablePoints[Index].Transform.SetLocation(CompoundNode->UpdateCenter(Context->CompoundGraph->PointsCompounds, Context->MainPoints));
		};

		if (!Context->Process(Initialize, ProcessNode, NumCompoundNodes)) { return false; }

		// Initiate merging
		Context->CompoundPointsBlender->PrepareMerge(Context->CompoundPoints, Context->CompoundGraph->PointsCompounds);
		Context->SetState(PCGExGraph::State_ProcessingCompound);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingCompound))
	{
		auto MergeCompound = [&](const int32 CompoundIndex)
		{
			Context->CompoundPointsBlender->MergeSingle(CompoundIndex, PCGExSettings::GetDistanceSettings(Settings->PointPointIntersectionSettings));
		};

		if (!Context->Process(MergeCompound, Context->CompoundGraph->NumNodes())) { return false; }

		Context->CompoundPointsBlender->Write();
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
	FProcessor::FProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges)
		: FClusterProcessor(InVtx, InEdges)
	{
		bBuildCluster = false;
	}

	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(FPCGExAsyncManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FuseClusters)

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		// Prepare insertion

		if (!BuildIndexedEdges(*EdgesIO, *EndpointsLookup, IndexedEdges, true)) { return false; }
		if (IndexedEdges.IsEmpty()) { return false; }

		bInvalidEdges = false;

		const TArray<FPCGPoint>& InPoints = VtxIO->GetIn()->GetPoints();

		IndexedEdges.Sort(
			[&](const PCGExGraph::FIndexedEdge& A, const PCGExGraph::FIndexedEdge& B)
			{
				const FVector AStart = InPoints[A.Start].Transform.GetLocation();
				const FVector BStart = InPoints[B.Start].Transform.GetLocation();

				int32 Result = AStart.X == BStart.X ? 0 : AStart.X < BStart.X ? 1 : -1;
				if (Result != 0) { return Result == 1; }

				Result = AStart.Y == BStart.Y ? 0 : AStart.Y < BStart.Y ? 1 : -1;
				if (Result != 0) { return Result == 1; }

				Result = AStart.Z == BStart.Z ? 0 : AStart.Z < BStart.Z ? 1 : -1;
				if (Result != 0) { return Result == 1; }

				const FVector AEnd = InPoints[A.End].Transform.GetLocation();
				const FVector BEnd = InPoints[B.End].Transform.GetLocation();

				Result = AEnd.X == BEnd.X ? 0 : AEnd.X < BEnd.X ? 1 : -1;
				if (Result != 0) { return Result == 1; }

				Result = AEnd.Y == BEnd.Y ? 0 : AEnd.Y < BEnd.Y ? 1 : -1;
				if (Result != 0) { return Result == 1; }

				Result = AEnd.Z == BEnd.Z ? 0 : AEnd.Z < BEnd.Z ? 1 : -1;
				return Result == 1;
			});

		return true;
	}

	void FProcessor::CompleteWork()
	{
		if (bInvalidEdges) { return; }

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FuseClusters)

		const TArray<FPCGPoint>& InPoints = VtxIO->GetIn()->GetPoints();

		const int32 VtxIOIndex = VtxIO->IOIndex;
		const int32 EdgesIOIndex = EdgesIO->IOIndex;

		for (const PCGExGraph::FIndexedEdge& Edge : IndexedEdges)
		{
			TypedContext->CompoundGraph->CreateBridgeUnsafe(
				InPoints[Edge.Start], VtxIOIndex, Edge.Start,
				InPoints[Edge.End], VtxIOIndex, Edge.End, EdgesIOIndex, Edge.PointIndex);
		}
	}

	FFuseBatch::FFuseBatch(FPCGContext* InContext, PCGExData::FPointIO* InVtx, const TArrayView<PCGExData::FPointIO*> InEdges):
		TBatch<PCGExFuseClusters::FProcessor>(InContext, InVtx, InEdges)
	{
		bInlineCompletion = true; // Points need to be added in the same order to yield deterministic results
	}

	FFuseBatch::~FFuseBatch()
	{
	}
}

#undef LOCTEXT_NAMESPACE
