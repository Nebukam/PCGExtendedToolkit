// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFindNodeStates.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE FindNodeStates

int32 UPCGExFindNodeStatesSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_M; }

PCGExData::EInit UPCGExFindNodeStatesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }
PCGExData::EInit UPCGExFindNodeStatesSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::Forward; }

TArray<FPCGPinProperties> UPCGExFindNodeStatesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExCluster::SourceNodeStateLabel, "Node states.", Required, {})
	return PinProperties;
}

FPCGExFindNodeStatesContext::~FPCGExFindNodeStatesContext()
{
	PCGEX_TERMINATE_ASYNC
	PCGEX_DELETE(StatesManager)
	StateFactories.Empty();
	NodeIndices.Empty();
}


PCGEX_INITIALIZE_ELEMENT(FindNodeStates)

bool FPCGExFindNodeStatesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindNodeStates)

	return PCGExDataState::GetInputStateFactories(
		Context, PCGExCluster::SourceNodeStateLabel,
		Context->StateFactories, {PCGExFactories::EType::NodeState}, Settings->bAllowStateOverlap);
}

bool FPCGExFindNodeStatesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindNodeStatesElement::Execute);

	// TODO : Need to refactor state system.

	PCGEX_CONTEXT_AND_SETTINGS(FindNodeStates)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExFindNodeState::FProcessorBatch>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExFindNodeState::FProcessorBatch* NewBatch)
			{
				//NewBatch->bRequiresWriteStep = true;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }


	if (Context->IsDone())
	{
		Context->OutputPointsAndEdges();
		Context->ExecuteEnd();
	}

	return Context->IsDone();
}

namespace PCGExFindNodeState
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(StatesManager)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FindNodeStates)

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		Cluster->GetNodePointIndices(NodePointIndices);

		StatesManager = new PCGExDataState::TStatesManager(VtxIO);
		StatesManager->RegisterAndCapture<UPCGExNodeStateFactory>(
			Context, TypedContext->StateFactories, [&](PCGExDataFilter::TFilter* Handler)
			{
				PCGExCluster::FNodeStateHandler* NodeStateHandler = static_cast<PCGExCluster::FNodeStateHandler*>(Handler);
				NodeStateHandler->CaptureCluster(Context, Cluster);
			});

		if (!StatesManager->bValid)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some input points could not be used with any graph."));
			PCGEX_DELETE(StatesManager)
			return false;
		}

		if (StatesManager->bHasPartials)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some input points only have partial metadata."));
		}

		if (StatesManager->PrepareForTesting(NodePointIndices))
		{
			bRequiresPrep = true;
			for (int i = 0; i < NodePointIndices.Num(); i++) { StatesManager->PrepareSingle(i); }
			StatesManager->PreparationComplete();
		}

		StartParallelLoopForNodes();

		return true;
	}

	void FProcessor::ProcessSingleNode(PCGExCluster::FNode& Node) { StatesManager->Test(Node.PointIndex); }

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration)
	{
		StatesManager->WriteStateAttributes(Cluster->Nodes[Iteration].PointIndex);
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FindNodeStates)

		if (Settings->bWriteStateName)
		{
			StatesManager->WriteStateNames(AsyncManagerPtr, Settings->StateNameAttributeName, Settings->StatelessName, NodePointIndices);
		}

		if (Settings->bWriteStateValue)
		{
			StatesManager->WriteStateValues(AsyncManagerPtr, Settings->StateValueAttributeName, Settings->StatelessValue, NodePointIndices);
		}

		if (Settings->bWriteEachStateIndividually)
		{
			StatesManager->WriteStateIndividualStates(AsyncManagerPtr, NodePointIndices);
		}

		StatesManager->WritePrepareForStateAttributes(Context);
		StartParallelLoopForRange(NumNodes);
	}

	void FProcessor::Write()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FindNodeStates)
	}

	//////// BATCH

	FProcessorBatch::FProcessorBatch(FPCGContext* InContext, PCGExData::FPointIO* InVtx, const TArrayView<PCGExData::FPointIO*> InEdges):
		TBatch(InContext, InVtx, InEdges)
	{
	}

	FProcessorBatch::~FProcessorBatch()
	{
	}

	bool FProcessorBatch::PrepareProcessing()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FindNodeStates)

		if (!TBatch::PrepareProcessing()) { return false; }

		return true;
	}

	bool FProcessorBatch::PrepareSingle(FProcessor* ClusterProcessor)
	{
		return true;
	}

	void FProcessorBatch::Write()
	{
		TBatch<FProcessor>::Write();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
