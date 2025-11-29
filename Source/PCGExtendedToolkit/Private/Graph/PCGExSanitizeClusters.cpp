// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExSanitizeClusters.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Graph/PCGExGraph.h"


#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EIOInit UPCGExSanitizeClustersSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }
PCGExData::EIOInit UPCGExSanitizeClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::NoInit; }

#pragma endregion

PCGEX_INITIALIZE_ELEMENT(SanitizeClusters)
PCGEX_ELEMENT_BATCH_EDGE_IMPL_ADV(SanitizeClusters)

bool FPCGExSanitizeClustersElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SanitizeClusters)
	PCGEX_FWD(GraphBuilderDetails)

	return true;
}

bool FPCGExSanitizeClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSanitizeClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SanitizeClusters)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
			{
				NewBatch->GraphBuilderDetails = Context->GraphBuilderDetails;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->OutputBatches();
	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExSanitizeClusters
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSanitizeClusters::Process);

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		TArray<PCGExGraph::FEdge> IndexedEdges;

		BuildIndexedEdges(EdgeDataFacade->Source, *EndpointsLookup, IndexedEdges);

		if (IndexedEdges.IsEmpty()) { return false; }

		GraphBuilder->Graph->InsertEdges(IndexedEdges);
		EdgeDataFacade->Source->ClearCachedKeys();
		return true;
	}

	void FBatch::OnInitialPostProcess()
	{
		TBatch<FProcessor>::OnInitialPostProcess();
		GraphBuilder->Compile(AsyncManager, true);
	}

	void FBatch::Output()
	{
		if (GraphBuilder->bCompiledSuccessfully) { GraphBuilder->StageEdgesOutputs(); }
		else { VtxDataFacade->Source->InitializeOutput(PCGExData::EIOInit::NoInit); }
	}
}

#undef LOCTEXT_NAMESPACE
