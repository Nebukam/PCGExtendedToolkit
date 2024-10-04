// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/States/PCGExFlagNodes.h"


#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Graph/PCGExCluster.h"
#include "Graph/States/PCGExClusterStates.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE FlagNodes

int32 UPCGExFlagNodesSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_M; }

PCGExData::EInit UPCGExFlagNodesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }
PCGExData::EInit UPCGExFlagNodesSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::Forward; }

TArray<FPCGPinProperties> UPCGExFlagNodesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExCluster::SourceNodeFlagLabel, "Node states.", Required, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(FlagNodes)

bool FPCGExFlagNodesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FlagNodes)

	return PCGExFactories::GetInputFactories(
		Context, PCGExCluster::SourceNodeFlagLabel, Context->StateFactories,
		{PCGExFactories::EType::StateNode}, true);
}

bool FPCGExFlagNodesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFlagNodesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FlagNodes)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExFlagNodes::FProcessorBatch>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExFlagNodes::FProcessorBatch>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
				NewBatch->bWriteVtxDataFacade = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}

namespace PCGExFlagNodes
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFindNodeState::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		ExpandedNodes = Cluster->ExpandedNodes;

		if (!ExpandedNodes)
		{
			ExpandedNodes = Cluster->GetExpandedNodes(false);
			bBuildExpandedNodes = true;
		}

		Cluster->ComputeEdgeLengths();

		StateManager = MakeUnique<PCGExClusterStates::FStateManager>(StateFlags, Cluster, VtxDataFacade, EdgeDataFacade);
		StateManager->Init(ExecutionContext, Context->StateFactories);

		if (bBuildExpandedNodes) { StartParallelLoopForRange(NumNodes); }
		else { StartParallelLoopForNodes(); }

		return true;
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 Count)
	{
		(*ExpandedNodes)[Iteration] = PCGExCluster::FExpandedNode(Cluster.Get(), Iteration);
	}

	void FProcessor::ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const int32 LoopIdx, const int32 Count)
	{
		StateManager->Test(Node);
	}

	void FProcessor::CompleteWork()
	{
		if (bBuildExpandedNodes) { StartParallelLoopForNodes(); }
	}

	void FProcessor::Write()
	{
	}

	//////// BATCH

	FProcessorBatch::FProcessorBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges):
		TBatch(InContext, InVtx, InEdges)
	{
	}

	FProcessorBatch::~FProcessorBatch()
	{
		StateFlags = nullptr;
	}

	void FProcessorBatch::OnProcessingPreparationComplete()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FlagNodes)

		const TSharedPtr<PCGExData::TBuffer<int64>> Writer = VtxDataFacade->GetWritable(Settings->FlagAttribute, Settings->InitialFlags, false, false);
		StateFlags = Writer->GetOutValues();

		TBatch<FProcessor>::OnProcessingPreparationComplete();
	}

	bool FProcessorBatch::PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor)
	{
		ClusterProcessor->StateFlags = StateFlags;
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
