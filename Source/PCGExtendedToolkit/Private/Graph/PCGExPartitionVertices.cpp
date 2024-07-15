// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExPartitionVertices.h"


#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExPartitionVerticesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }
PCGExData::EInit UPCGExPartitionVerticesSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::Forward; }

#pragma endregion

FPCGExPartitionVerticesContext::~FPCGExPartitionVerticesContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(VtxPartitions)

	IndexedEdges.Empty();
}

PCGEX_INITIALIZE_ELEMENT(PartitionVertices)

bool FPCGExPartitionVerticesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PartitionVertices)

	Context->VtxPartitions = new PCGExData::FPointIOCollection();
	Context->VtxPartitions->DefaultOutputLabel = PCGExGraph::OutputVerticesLabel;

	return true;
}

bool FPCGExPartitionVerticesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPartitionVerticesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PartitionVertices)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatch<PCGExPartitionVertices::FProcessor>>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExClusterMT::TBatch<PCGExPartitionVertices::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}

		Context->VtxPartitions->Pairs.Reserve(Context->GetTotalNumProcessors());
	}

	if (!Context->ProcessClusters()) { return false; }

	Context->OutputBatches();
	Context->VtxPartitions->OutputTo(Context);
	Context->MainEdges->OutputTo(Context);

	return Context->TryComplete();
}

namespace PCGExPartitionVertices
{
	PCGExCluster::FCluster* FProcessor::HandleCachedCluster(const PCGExCluster::FCluster* InClusterRef)
	{
		// Create a heavy copy we'll update and forward
		bDeleteCluster = false;
		return new PCGExCluster::FCluster(InClusterRef, VtxIO, EdgesIO, true, true, true);
	}

	FProcessor::~FProcessor()
	{
		Remapping.Empty();
		KeptIndices.Empty();
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPartitionVertices::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PartitionVertices)

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		LocalTypedContext = TypedContext;

		Cluster->NodeIndexLookup->Empty();

		PointPartitionIO = new PCGExData::FPointIO(VtxIO);
		PointPartitionIO->InitializeOutput(PCGExData::EInit::NewOutput);
		TArray<FPCGPoint>& MutablePoints = PointPartitionIO->GetOut()->GetMutablePoints();

		MutablePoints.SetNumUninitialized(NumNodes);
		KeptIndices.SetNumUninitialized(NumNodes);
		Remapping.Reserve(NumNodes);

		Cluster->WillModifyVtxIO();

		Cluster->VtxIO = PointPartitionIO;
		Cluster->NodeIndexLookup->Reserve(NumNodes);
		Cluster->NumRawVtx = NumNodes;

		for (PCGExCluster::FNode& Node : (*Cluster->Nodes))
		{
			int32 i = Node.NodeIndex;

			KeptIndices[i] = Node.PointIndex;
			Remapping.Add(Node.PointIndex, i);
			Cluster->NodeIndexLookup->Add(i, i); // most useless lookup in history of lookups

			Node.PointIndex = i;
		}

		StartParallelLoopForNodes();
		StartParallelLoopForEdges();

		return true;
	}

	void FProcessor::ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node)
	{
		PointPartitionIO->GetOut()->GetMutablePoints()[Node.NodeIndex] = VtxIO->GetInPoint(KeptIndices[Node.NodeIndex]);
	}

	void FProcessor::ProcessSingleEdge(PCGExGraph::FIndexedEdge& Edge)
	{
		Edge.Start = Remapping[Edge.Start];
		Edge.End = Remapping[Edge.End];
	}

	void FProcessor::CompleteWork()
	{
		FString OutId;
		PCGExGraph::SetClusterVtx(PointPartitionIO, OutId);
		PCGExGraph::MarkClusterEdges(EdgesIO, OutId);

		ForwardCluster(true);
	}

	void FProcessor::Output()
	{
		LocalTypedContext->VtxPartitions->AddUnsafe(PointPartitionIO);
	}
}

#undef LOCTEXT_NAMESPACE
