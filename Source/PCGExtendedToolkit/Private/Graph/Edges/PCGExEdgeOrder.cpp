// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExEdgeOrder.h"

#include "Data/Blending/PCGExMetadataBlender.h"

#define LOCTEXT_NAMESPACE "PCGExEdgesToPaths"
#define PCGEX_NAMESPACE EdgeOrder

PCGExData::EIOInit UPCGExEdgeOrderSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Forward; }
PCGExData::EIOInit UPCGExEdgeOrderSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_INITIALIZE_ELEMENT(EdgeOrder)

bool FPCGExEdgeOrderElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(EdgeOrder)

	return true;
}

bool FPCGExEdgeOrderElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExEdgeOrderElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(EdgeOrder)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExEdgeOrder::FBatch>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExEdgeOrder::FBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}


namespace PCGExEdgeOrder
{
	FProcessor::~FProcessor()
	{
	}

	TSharedPtr<PCGExCluster::FCluster> FProcessor::HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef)
	{
		// Create a lite copy with only edges edited, we'll forward that to the output
		return MakeShared<PCGExCluster::FCluster>(InClusterRef, VtxDataFacade->Source, EdgeDataFacade->Source, NodeIndexLookup, false, true, true);
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExEdgeOrder::Process);

		EdgeDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		if (!DirectionSettings.InitFromParent(ExecutionContext, StaticCastWeakPtr<FBatch>(ParentBatch).Pin()->DirectionSettings, EdgeDataFacade))
		{
			return false;
		}

		VtxEndpointBuffer = VtxDataFacade->GetReadable<int64>(PCGExGraph::Tag_VtxEndpoint);
		EndpointsBuffer = EdgeDataFacade->GetWritable<int64>(PCGExGraph::Tag_EdgeEndpoints, PCGExData::EBufferInit::New);

		StartParallelLoopForEdges();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForEdges(const PCGExMT::FScope& Scope)
	{
		FClusterProcessor::PrepareSingleLoopScopeForEdges(Scope);
		EdgeDataFacade->Fetch(Scope);
	}

	void FProcessor::ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FEdge& Edge, const PCGExMT::FScope& Scope)
	{
		const PCGExGraph::FEdge PreviousData = Edge;
		DirectionSettings.SortEndpoints(Cluster.Get(), Edge);

		const PCGExCluster::FNode& StartNode = *Cluster->GetEdgeStart(Edge);
		uint32 StartID = 0;
		uint32 StartAdjacency = 0;
		PCGEx::H64(VtxEndpointBuffer->Read(StartNode.PointIndex), StartID, StartAdjacency);

		const PCGExCluster::FNode& EndNode = *Cluster->GetEdgeEnd(Edge);
		uint32 EndID = 0;
		uint32 EndAdjacency = 0;
		PCGEx::H64(VtxEndpointBuffer->Read(EndNode.PointIndex), EndID, EndAdjacency);

		EndpointsBuffer->GetMutable(EdgeIndex) = PCGEx::H64(StartID, EndID); // Rewrite endpoints data as ordered
	}

	void FProcessor::CompleteWork()
	{
		EdgeDataFacade->Write(AsyncManager);
		ForwardCluster();
	}

	void FBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TBatch<FProcessor>::RegisterBuffersDependencies(FacadePreloader);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(EdgeOrder)
		FacadePreloader.Register<int64>(ExecutionContext, PCGExGraph::Tag_VtxEndpoint);
		DirectionSettings.RegisterBuffersDependencies(ExecutionContext, FacadePreloader);
	}

	void FBatch::OnProcessingPreparationComplete()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(EdgeOrder)

		DirectionSettings = Settings->DirectionSettings;

		if (!DirectionSettings.Init(ExecutionContext, VtxDataFacade, Context->GetEdgeSortingRules()))
		{
			bIsBatchValid = false;
			return;
		}

		TBatch<FProcessor>::OnProcessingPreparationComplete();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
