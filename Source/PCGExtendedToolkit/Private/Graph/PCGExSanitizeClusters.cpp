// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExSanitizeClusters.h"


#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExSanitizeClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }
PCGExData::EInit UPCGExSanitizeClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

#pragma endregion

PCGEX_INITIALIZE_ELEMENT(SanitizeClusters)

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

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExSanitizeClusters::FProcessorBatch>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExSanitizeClusters::FProcessorBatch>& NewBatch)
			{
				NewBatch->GraphBuilderDetails = Context->GraphBuilderDetails;
			}))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters(PCGExMT::State_Done)) { return false; }

	Context->OutputBatches();
	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExSanitizeClusters
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSanitizeClusters::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		TArray<PCGExGraph::FIndexedEdge> IndexedEdges;

		BuildIndexedEdges(EdgeDataFacade->Source, *EndpointsLookup, IndexedEdges);
		if (!IndexedEdges.IsEmpty()) { GraphBuilder->Graph->InsertEdges(IndexedEdges); }

		EdgeDataFacade->Source->CleanupKeys();

		return true;
	}

	void FProcessorBatch::CompleteWork()
	{
		GraphBuilder->Compile(AsyncManager, true);
		//TBatchWithGraphBuilder<FProcessor>::CompleteWork();
	}

	void FProcessorBatch::Output()
	{
		if (GraphBuilder->bCompiledSuccessfully) { GraphBuilder->OutputEdgesToContext(); }
		else { GraphBuilder->NodeDataFacade->Source->InitializeOutput(PCGExData::EInit::NoOutput); }
		//TBatchWithGraphBuilder<FProcessor>::Output();
	}
}

#undef LOCTEXT_NAMESPACE
