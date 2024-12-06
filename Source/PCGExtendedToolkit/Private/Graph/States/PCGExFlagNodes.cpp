// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/States/PCGExFlagNodes.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Graph/PCGExCluster.h"
#include "Graph/States/PCGExClusterStates.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE FlagNodes

int32 UPCGExFlagNodesSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_M; }

PCGExData::EIOInit UPCGExFlagNodesSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }
PCGExData::EIOInit UPCGExFlagNodesSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Forward; }

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
		{PCGExFactories::EType::NodeState}, true);
}

bool FPCGExFlagNodesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFlagNodesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FlagNodes)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExFlagNodes::FBatch>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExFlagNodes::FBatch>& NewBatch)
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

		Cluster->ComputeEdgeLengths();

		StateManager = MakeShared<PCGExClusterStates::FStateManager>(StateFlags, Cluster.ToSharedRef(), VtxDataFacade, EdgeDataFacade);
		StateManager->Init(ExecutionContext, Context->StateFactories);

		StartParallelLoopForNodes();

		return true;
	}

	void FProcessor::ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const PCGExMT::FScope& Scope)
	{
		StateManager->Test(Node);
	}

	void FProcessor::CompleteWork()
	{
	}

	void FProcessor::Write()
	{
	}

	//////// BATCH

	FBatch::~FBatch()
	{
		StateFlags = nullptr;
	}

	void FBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TBatch<FProcessor>::RegisterBuffersDependencies(FacadePreloader);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FlagNodes)
		PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, Context->StateFactories, FacadePreloader);
	}

	void FBatch::OnProcessingPreparationComplete()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FlagNodes)

		const TSharedPtr<PCGExData::TBuffer<int64>> Writer = VtxDataFacade->GetWritable(Settings->FlagAttribute, Settings->InitialFlags, false, PCGExData::EBufferInit::Inherit);
		StateFlags = Writer->GetOutValues();

		TBatch<FProcessor>::OnProcessingPreparationComplete();
	}

	bool FBatch::PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor)
	{
		ClusterProcessor->StateFlags = StateFlags;
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
