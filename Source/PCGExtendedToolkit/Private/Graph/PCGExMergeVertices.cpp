// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExMergeVertices.h"


#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExMergeVerticesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }
PCGExData::EInit UPCGExMergeVerticesSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

#pragma endregion

FPCGExMergeVerticesContext::~FPCGExMergeVerticesContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(CompositeIO)
	PCGEX_DELETE(Merger)
}

void FPCGExMergeVerticesContext::OnBatchesProcessingDone()
{
	Merger = new FPCGExPointIOMerger(CompositeIO);

	int32 StartOffset = 0;

	for (int i = 0; i < Batches.Num(); i++)
	{
		PCGExClusterMT::TBatch<PCGExMergeVertices::FProcessor>* Batch = static_cast<PCGExClusterMT::TBatch<PCGExMergeVertices::FProcessor>*>(Batches[i]);
		Merger->Append(Batch->VtxIO);

		for (PCGExMergeVertices::FProcessor* Processor : Batch->Processors) { Processor->StartIndexOffset = StartOffset; }
		StartOffset += Batch->VtxIO->GetNum();
	}

	Merger->Merge(GetAsyncManager(), &CarryOverDetails);
	PCGExGraph::SetClusterVtx(CompositeIO, OutVtxId); // After merge since it forwards IDs
}

void FPCGExMergeVerticesContext::OnBatchesCompletingWorkDone()
{
	Merger->Write(GetAsyncManager());
}

PCGEX_INITIALIZE_ELEMENT(MergeVertices)

bool FPCGExMergeVerticesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(MergeVertices)

	PCGEX_FWD(CarryOverDetails)
	Context->CarryOverDetails.Init();

	Context->CompositeIO = new PCGExData::FPointIO();
	Context->CompositeIO->SetInfos(0, PCGExGraph::OutputVerticesLabel);
	Context->CompositeIO->InitializeOutput<UPCGExClusterNodesData>(PCGExData::EInit::NewOutput);

	return true;
}

bool FPCGExMergeVerticesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExMergeVerticesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(MergeVertices)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatch<PCGExMergeVertices::FProcessor>>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExClusterMT::TBatch<PCGExMergeVertices::FProcessor>* NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	Context->CompositeIO->OutputTo(Context);
	Context->MainEdges->OutputTo(Context);

	return Context->TryComplete();
}

namespace PCGExMergeVertices
{
	PCGExCluster::FCluster* FProcessor::HandleCachedCluster(const PCGExCluster::FCluster* InClusterRef)
	{
		// Create a heavy copy we'll update and forward
		return new PCGExCluster::FCluster(InClusterRef, VtxIO, EdgesIO, true, true, true);
	}

	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExMergeVertices::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(MergeVertices)

		LocalTypedContext = TypedContext;

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		Cluster->WillModifyVtxIO();

		bDeleteCluster = false;
		return true;
	}

	void FProcessor::ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node)
	{
		Node.PointIndex += StartIndexOffset;
	}

	void FProcessor::ProcessSingleEdge(PCGExGraph::FIndexedEdge& Edge)
	{
		Edge.Start += StartIndexOffset;
		Edge.End += StartIndexOffset;
	}

	void FProcessor::CompleteWork()
	{
		TMap<int32, int32>* OffsetLookup = new TMap<int32, int32>();
		OffsetLookup->Reserve(Cluster->NodeIndexLookup->Num());
		for (const TPair<int32, int32>& Lookup : (*Cluster->NodeIndexLookup)) { OffsetLookup->Add(Lookup.Key + StartIndexOffset, Lookup.Value); }

		PCGEX_DELETE(Cluster->NodeIndexLookup)
		Cluster->NodeIndexLookup = OffsetLookup;

		StartParallelLoopForNodes();
		StartParallelLoopForEdges();
	}

	void FProcessor::Write()
	{
		Cluster->VtxIO = LocalTypedContext->CompositeIO;
		Cluster->NumRawVtx = LocalTypedContext->CompositeIO->GetNum(PCGExData::ESource::Out);

		EdgesIO->InitializeOutput(PCGExData::EInit::DuplicateInput);
		PCGExGraph::MarkClusterEdges(EdgesIO, LocalTypedContext->OutVtxId);

		ForwardCluster(true);
	}
}

#undef LOCTEXT_NAMESPACE
