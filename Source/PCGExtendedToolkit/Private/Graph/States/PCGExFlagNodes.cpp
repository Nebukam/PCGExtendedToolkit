﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/States/PCGExFlagNodes.h"

#include "Data/PCGExData.h"
#include "Graph/PCGExCluster.h"
#include "Graph/States/PCGExClusterStates.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE FlagNodes

PCGExData::EIOInit UPCGExFlagNodesSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }
PCGExData::EIOInit UPCGExFlagNodesSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Forward; }

TArray<FPCGPinProperties> UPCGExFlagNodesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_FACTORIES(PCGExCluster::SourceNodeFlagLabel, "Node states.", Required, FPCGExDataTypeInfoClusterState::AsId())
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(FlagNodes)
PCGEX_ELEMENT_BATCH_EDGE_IMPL_ADV(FlagNodes)

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
		if (!Context->StartProcessingClusters(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
				NewBatch->bWriteVtxDataFacade = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}

namespace PCGExFlagNodes
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFindNodeState::Process);

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		Cluster->ComputeEdgeLengths();

		StateManager = MakeShared<PCGExClusterStates::FStateManager>(StateFlags, Cluster.ToSharedRef(), VtxDataFacade, EdgeDataFacade);
		StateManager->Init(ExecutionContext, Context->StateFactories);

		StartParallelLoopForNodes();

		return true;
	}

	void FProcessor::ProcessNodes(const PCGExMT::FScope& Scope)
	{
		TArray<PCGExCluster::FNode>& Nodes = *Cluster->Nodes;

		PCGEX_SCOPE_LOOP(Index)
		{
			PCGExCluster::FNode& Node = Nodes[Index];

			StateManager->Test(Node);
		}
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
		const TSharedPtr<PCGExData::TArrayBuffer<int64>> ElementsWriter = StaticCastSharedPtr<PCGExData::TArrayBuffer<int64>>(Writer);
		StateFlags = ElementsWriter->GetOutValues();

		TBatch<FProcessor>::OnProcessingPreparationComplete();
	}

	bool FBatch::PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor)
	{
		if (!TBatch<FProcessor>::PrepareSingle(InProcessor)) { return false; }
		PCGEX_TYPED_PROCESSOR
		TypedProcessor->StateFlags = StateFlags;
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
