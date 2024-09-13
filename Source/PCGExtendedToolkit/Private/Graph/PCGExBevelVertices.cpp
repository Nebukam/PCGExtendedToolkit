// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBevelVertices.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Graph/PCGExCluster.h"
#include "Graph/States/PCGExClusterStates.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BevelVertices

int32 UPCGExBevelVerticesSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_M; }

PCGExData::EInit UPCGExBevelVerticesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }
PCGExData::EInit UPCGExBevelVerticesSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::Forward; }

TArray<FPCGPinProperties> UPCGExBevelVerticesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExPointFilter::SourceFiltersLabel, "Bevel conditions", Normal, {})
	return PinProperties;
}

FPCGExBevelVerticesContext::~FPCGExBevelVerticesContext()
{
	PCGEX_TERMINATE_ASYNC
	StateFactories.Empty();
}

PCGEX_INITIALIZE_ELEMENT(BevelVertices)

bool FPCGExBevelVerticesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BevelVertices)

	return PCGExFactories::GetInputFactories(
		Context, PCGExCluster::SourceNodeFlagLabel, Context->StateFactories,
		{PCGExFactories::EType::StateNode}, true);
}

bool FPCGExBevelVerticesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBevelVerticesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BevelVertices)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExBevelVertices::FProcessorBatch>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExBevelVertices::FProcessorBatch* NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
				NewBatch->bWriteVtxDataFacade = true;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}

namespace PCGExBevelVertices
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(FilterManager)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBevelVertices::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BevelVertices)

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		ExpandedNodes = Cluster->ExpandedNodes;

		if (!ExpandedNodes)
		{
			ExpandedNodes = Cluster->GetExpandedNodes(false);
			bBuildExpandedNodes = true;
		}

		Cluster->ComputeEdgeLengths();

		FilterManager = new PCGExClusterStates::FStateManager(StateFlags, Cluster, VtxDataFacade, EdgeDataFacade);
		FilterManager->Init(Context, TypedContext->StateFactories);

		if (bBuildExpandedNodes) { StartParallelLoopForRange(NumNodes); }
		else { StartParallelLoopForNodes(); }

		return true;
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 Count)
	{
		(*ExpandedNodes)[Iteration] = new PCGExCluster::FExpandedNode(Cluster, Iteration);
	}

	void FProcessor::ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const int32 LoopIdx, const int32 Count)
	{
		FilterManager->Test(Node);
	}

	void FProcessor::CompleteWork()
	{
		if (bBuildExpandedNodes) { StartParallelLoopForNodes(); }
	}

	void FProcessor::Write()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BevelVertices)
	}

	//////// BATCH

	FProcessorBatch::FProcessorBatch(FPCGContext* InContext, PCGExData::FPointIO* InVtx, const TArrayView<PCGExData::FPointIO*> InEdges):
		TBatch(InContext, InVtx, InEdges)
	{
	}

	FProcessorBatch::~FProcessorBatch()
	{
		StateFlags = nullptr;
	}

	bool FProcessorBatch::PrepareProcessing()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BevelVertices)

		if (!TBatch::PrepareProcessing()) { return false; }

		PCGEx::TAttributeWriter<int64>* Writer = VtxDataFacade->GetWriter(Settings->FlagAttribute, Settings->InitialFlags, false, false);
		StateFlags = &Writer->Values;

		return true;
	}

	bool FProcessorBatch::PrepareSingle(FProcessor* ClusterProcessor)
	{
		ClusterProcessor->StateFlags = StateFlags;
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
