// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExEdgeOrder.h"


#include "Data/PCGExData.h"
#include "Data/Utils/PCGExDataPreloader.h"
#include "Clusters/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "EdgeOrder"
#define PCGEX_NAMESPACE EdgeOrder

PCGExData::EIOInit UPCGExEdgeOrderSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Forward; }
PCGExData::EIOInit UPCGExEdgeOrderSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_INITIALIZE_ELEMENT(EdgeOrder)
PCGEX_ELEMENT_BATCH_EDGE_IMPL_ADV(EdgeOrder)

bool FPCGExEdgeOrderElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExClustersProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(EdgeOrder)

	return true;
}

bool FPCGExEdgeOrderElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExEdgeOrderElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(EdgeOrder)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters([](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; }, [&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
		{
		}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}


namespace PCGExEdgeOrder
{
	FProcessor::~FProcessor()
	{
	}

	TSharedPtr<PCGExClusters::FCluster> FProcessor::HandleCachedCluster(const TSharedRef<PCGExClusters::FCluster>& InClusterRef)
	{
		// Create a lite copy with only edges edited, we'll forward that to the output
		return MakeShared<PCGExClusters::FCluster>(InClusterRef, VtxDataFacade->Source, EdgeDataFacade->Source, NodeIndexLookup, false, true, true);
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExEdgeOrder::Process);

		EdgeDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InTaskManager)) { return false; }

		if (!DirectionSettings.InitFromParent(ExecutionContext, GetParentBatch<FBatch>()->DirectionSettings, EdgeDataFacade))
		{
			return false;
		}

		VtxEndpointBuffer = VtxDataFacade->GetReadable<int64>(PCGExClusters::Labels::Attr_PCGExVtxIdx);
		EndpointsBuffer = EdgeDataFacade->GetWritable<int64>(PCGExClusters::Labels::Attr_PCGExEdgeIdx, PCGExData::EBufferInit::New);

		StartParallelLoopForEdges();

		return true;
	}

	void FProcessor::ProcessEdges(const PCGExMT::FScope& Scope)
	{
		EdgeDataFacade->Fetch(Scope);

		TArray<PCGExGraphs::FEdge>& ClusterEdges = *Cluster->Edges;

		PCGEX_SCOPE_LOOP(Index)
		{
			PCGExGraphs::FEdge& Edge = ClusterEdges[Index];

			DirectionSettings.SortEndpoints(Cluster.Get(), Edge);

			uint32 StartID = 0;
			uint32 StartAdjacency = 0;
			PCGEx::H64(VtxEndpointBuffer->Read(Edge.Start), StartID, StartAdjacency);

			uint32 EndID = 0;
			uint32 EndAdjacency = 0;
			PCGEx::H64(VtxEndpointBuffer->Read(Edge.End), EndID, EndAdjacency);

			EndpointsBuffer->SetValue(Index, PCGEx::H64(StartID, EndID)); // Rewrite endpoints data as ordered
		}
	}

	void FProcessor::CompleteWork()
	{
		EdgeDataFacade->WriteFastest(TaskManager);
		ForwardCluster();
	}

	void FBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TBatch<FProcessor>::RegisterBuffersDependencies(FacadePreloader);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(EdgeOrder)
		FacadePreloader.Register<int64>(ExecutionContext, PCGExClusters::Labels::Attr_PCGExVtxIdx);
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
