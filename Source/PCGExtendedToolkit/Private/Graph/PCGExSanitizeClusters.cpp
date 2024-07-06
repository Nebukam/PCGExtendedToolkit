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

	PCGEX_DELETE_TARRAY(Builders)

	for (TMap<uint32, int32>& Map : EndpointsLookups) { Map.Empty(); }
	EndpointsLookups.Empty();
}

PCGEX_INITIALIZE_ELEMENT(SanitizeClusters)

bool FPCGExSanitizeClustersElement::Boot(FPCGContext* InContext) const
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
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		Context->bBuildEndpointsLookup = false;

		int32 BuildIndex = 0;
		while (Context->AdvancePointsIO(false))
		{
			if (!Context->TaggedEdges) { continue; }
			Context->EndpointsLookups.Emplace();
			Context->Builders.Add(nullptr);
			Context->GetAsyncManager()->Start<FPCGExSanitizeClusterTask>(BuildIndex++, Context->CurrentIO, Context->TaggedEdges);
		}

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncCompletion);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncCompletion))
	{
		PCGEX_ASYNC_WAIT

		for (PCGExGraph::FGraphBuilder* Builder : Context->Builders) { Builder->CompileAsync(Context->GetAsyncManager()); }

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_ASYNC_WAIT

		for (const PCGExGraph::FGraphBuilder* Builder : Context->Builders)
		{
			if (!Builder) { continue; }
			if (!Builder->bCompiledSuccessfully)
			{
				Builder->PointIO->InitializeOutput(PCGExData::EInit::NoOutput);
				continue;
			}
			Builder->Write(Context);
		}

		Context->Done();
		Context->OutputMainPoints();
	}

	return Context->TryComplete();
}

bool FPCGExSanitizeClusterTask::ExecuteTask()
{
	FPCGExSanitizeClustersContext* Context = Manager->GetContext<FPCGExSanitizeClustersContext>();
	PCGEX_SETTINGS(SanitizeClusters)

	Context->Builders[TaskIndex] = new PCGExGraph::FGraphBuilder(PointIO, &Context->GraphBuilderDetails, 6, Context->MainEdges);

	TMap<uint32, int32>& Map = Context->EndpointsLookups[TaskIndex];
	TArray<int32> Adjacency;

	PCGExGraph::BuildEndpointsLookup(PointIO, Map, Adjacency);

	for (PCGExData::FPointIO* Edges : TaggedEdges->Entries)
	{
		InternalStart<FPCGExSanitizeInsertTask>(TaskIndex, PointIO, Edges);
	}

	return true;
}

bool FPCGExSanitizeInsertTask::ExecuteTask()
{
	FPCGExSanitizeClustersContext* Context = Manager->GetContext<FPCGExSanitizeClustersContext>();
	PCGEX_SETTINGS(SanitizeClusters)

	const PCGExGraph::FGraphBuilder* Builder = Context->Builders[TaskIndex];

	TArray<PCGExGraph::FIndexedEdge> IndexedEdges;
	EdgeIO->CreateInKeys();

	BuildIndexedEdges(EdgeIO, Context->EndpointsLookups[TaskIndex], IndexedEdges);
	if (!IndexedEdges.IsEmpty()) { Builder->Graph->InsertEdges(IndexedEdges); }

	EdgeIO->CleanupKeys();

	return true;
}

#undef LOCTEXT_NAMESPACE
