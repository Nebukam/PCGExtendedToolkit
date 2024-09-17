// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExSanitizeClusters.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExSanitizeClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }
PCGExData::EInit UPCGExSanitizeClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

#pragma endregion

FPCGExSanitizeClustersContext::~FPCGExSanitizeClustersContext()
{
	PCGEX_TERMINATE_ASYNC
}

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

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExSanitizeClusters::FProcessorBatch>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExSanitizeClusters::FProcessorBatch* NewBatch)
			{
				NewBatch->GraphBuilderDetails = Context->GraphBuilderDetails;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	Context->OutputBatches();
	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExSanitizeClusters
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSanitizeClusters::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SanitizeClusters)

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		TArray<PCGExGraph::FIndexedEdge> IndexedEdges;
		EdgesIO->CreateInKeys();

		BuildIndexedEdges(EdgesIO, *EndpointsLookup, IndexedEdges);
		if (!IndexedEdges.IsEmpty()) { GraphBuilder->Graph->InsertEdges(IndexedEdges); }

		EdgesIO->CleanupKeys();

		return true;
	}

	void FProcessorBatch::CompleteWork()
	{
		GraphBuilder->Compile(AsyncManagerPtr);
		//TBatchWithGraphBuilder<FProcessor>::CompleteWork();
	}

	void FProcessorBatch::Output()
	{
		if (GraphBuilder->bCompiledSuccessfully) { GraphBuilder->Write(); }
		else { GraphBuilder->PointIO->InitializeOutput(PCGExData::EInit::NoOutput); }
		//TBatchWithGraphBuilder<FProcessor>::Output();
	}
}

#undef LOCTEXT_NAMESPACE
