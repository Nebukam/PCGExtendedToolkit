// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExMergeVertices.h"


#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EIOInit UPCGExMergeVerticesSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::NoOutput; }
PCGExData::EIOInit UPCGExMergeVerticesSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::NoOutput; }

#pragma endregion

void FPCGExMergeVerticesContext::ClusterProcessing_InitialProcessingDone()
{
	Merger = MakeShared<FPCGExPointIOMerger>(CompositeIODataFacade.ToSharedRef());

	int32 StartOffset = 0;

	for (int i = 0; i < Batches.Num(); i++)
	{
		PCGExClusterMT::TBatch<PCGExMergeVertices::FProcessor>* Batch = static_cast<PCGExClusterMT::TBatch<PCGExMergeVertices::FProcessor>*>(Batches[i].Get());
		Merger->Append(Batch->VtxDataFacade->Source);

		for (const TSharedRef<PCGExMergeVertices::FProcessor>& Processor : Batch->Processors) { Processor->StartIndexOffset = StartOffset; }
		StartOffset += Batch->VtxDataFacade->GetNum();
	}

	Merger->Merge(GetAsyncManager(), &CarryOverDetails);
	PCGExGraph::SetClusterVtx(CompositeIODataFacade->Source, OutVtxId); // After merge since it forwards IDs
}

void FPCGExMergeVerticesContext::ClusterProcessing_WorkComplete()
{
	CompositeIODataFacade->Write(GetAsyncManager());
}

PCGEX_INITIALIZE_ELEMENT(MergeVertices)

bool FPCGExMergeVerticesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(MergeVertices)

	PCGEX_FWD(CarryOverDetails)
	Context->CarryOverDetails.Init();

	TSharedPtr<PCGExData::FPointIO> CompositeIO = PCGExData::NewPointIO(Context, PCGExGraph::OutputVerticesLabel, 0);
	Context->CompositeIODataFacade = MakeShared<PCGExData::FFacade>(CompositeIO.ToSharedRef());
	CompositeIO->InitializeOutput<UPCGExClusterNodesData>(PCGExData::EIOInit::NewOutput);

	return true;
}

bool FPCGExMergeVerticesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExMergeVerticesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(MergeVertices)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatch<PCGExMergeVertices::FProcessor>>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::TBatch<PCGExMergeVertices::FProcessor>>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)

	Context->CompositeIODataFacade->Source->StageOutput();
	Context->MainEdges->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExMergeVertices
{
	TSharedPtr<PCGExCluster::FCluster> FProcessor::HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef)
	{
		// Create a heavy copy we'll update and forward
		return MakeShared<PCGExCluster::FCluster>(
			InClusterRef, VtxDataFacade->Source, EdgeDataFacade->Source, NodeIndexLookup,
			true, true, true);
	}

	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExMergeVertices::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		Cluster->WillModifyVtxIO();

		return true;
	}

	void FProcessor::ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const int32 LoopIdx, const int32 Count)
	{
		Node.PointIndex += StartIndexOffset;
	}

	void FProcessor::ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FEdge& Edge, const int32 LoopIdx, const int32 Count)
	{
		Edge.Start += StartIndexOffset;
		Edge.End += StartIndexOffset;
	}

	void FProcessor::CompleteWork()
	{
		StartParallelLoopForNodes();
		StartParallelLoopForEdges();
	}

	void FProcessor::Write()
	{
		Cluster->VtxIO = Context->CompositeIODataFacade->Source;
		Cluster->NumRawVtx = Context->CompositeIODataFacade->Source->GetNum(PCGExData::ESource::Out);

		EdgeDataFacade->Source->InitializeOutput(PCGExData::EIOInit::DuplicateInput);
		PCGExGraph::MarkClusterEdges(EdgeDataFacade->Source, Context->OutVtxId);

		ForwardCluster();
	}
}

#undef LOCTEXT_NAMESPACE
