// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExPartitionVertices.h"

#include "Data/PCGExGraphDefinition.h"

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
	}

	if (!Context->ProcessClusters()) { return false; }

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_ASYNC_WAIT

		Context->VtxPartitions->OutputTo(Context);
		Context->MainEdges->OutputTo(Context);

		Context->Done();
		Context->ExecuteEnd();
	}

	return Context->IsDone();
}

namespace PCGExPartitionVertices
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
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PartitionVertices)

		if (FClusterProcessor::Process(AsyncManager)) { return false; }

		Cluster->NodeIndexLookup->Empty();

		PointPartitionIO = TypedContext->VtxPartitions->Emplace_GetRef(VtxIO, PCGExData::EInit::NewOutput);
		TArray<FPCGPoint>& MutablePoints = PointPartitionIO->GetOut()->GetMutablePoints();

		const int32 NumNodes = Cluster->Nodes->Num();
		MutablePoints.SetNumUninitialized(NumNodes);
		KeptIndices.SetNumUninitialized(NumNodes);
		Remapping.Reserve(NumNodes);
		Cluster->NodeIndexLookup->Reserve(NumNodes);

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

	void FProcessor::ProcessSingleNode(PCGExCluster::FNode& Node)
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
}

#undef LOCTEXT_NAMESPACE
