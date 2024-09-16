// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExRefineEdges.h"

#include "Graph/PCGExGraph.h"
#include "Graph/Edges/Refining/PCGExEdgeRefinePrimMST.h"
#include "Graph/Filters/PCGExClusterFilter.h"

#define LOCTEXT_NAMESPACE "PCGExRefineEdges"
#define PCGEX_NAMESPACE RefineEdges

TArray<FPCGPinProperties> UPCGExRefineEdgesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (Refinement && Refinement->RequiresHeuristics()) { PCGEX_PIN_PARAMS(PCGExGraph::SourceHeuristicsLabel, "Heuristics may be required by some refinements.", Required, {}) }
	if (Refinement && Refinement->SupportFilters())
	{
		//PCGEX_PIN_PARAMS(PCGExRefineEdges::SourceVtxFilters, "Filters used to check if a vtx should be processed.", Normal, {})
		PCGEX_PIN_PARAMS(PCGExRefineEdges::SourceEdgeFilters, "Refinements filters.", Normal, {})
	}

	if (Sanitization == EPCGExRefineSanitization::Filters)
	{
		PCGEX_PIN_PARAMS(PCGExRefineEdges::SourceSanitizeEdgeFilters, "Filters that define which edges are to be kept. During the sanitization step, edges that pass the filters are restored if they were previously removed.", Normal, {})
	}

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExRefineEdgesSettings::OutputPinProperties() const
{
	if (!bOutputOnlyEdgesAsPoints) { return Super::OutputPinProperties(); }
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Edges but as simple points.", Required, {})
	return PinProperties;
}

PCGExData::EInit UPCGExRefineEdgesSettings::GetMainOutputInitMode() const { return bOutputOnlyEdgesAsPoints ? PCGExData::EInit::NoOutput : PCGExData::EInit::NewOutput; }
PCGExData::EInit UPCGExRefineEdgesSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(RefineEdges)

FPCGExRefineEdgesContext::~FPCGExRefineEdgesContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExRefineEdgesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(RefineEdges)

	if (!Settings->Refinement)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("No refinement selected."));
		return false;
	}

	PCGEX_OPERATION_BIND(Refinement, UPCGExEdgeRefineOperation)
	PCGEX_FWD(GraphBuilderDetails)

	if (Context->Refinement->RequiresHeuristics() && !Context->bHasValidHeuristics)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("The selected refinement requires heuristics to be connected, but none can be found."));
		return false;
	}

	if (Context->Refinement->SupportFilters())
	{
		//GetInputFactories(Context, PCGExRefineEdges::SourceVtxFilters, Context->VtxFilterFactories, PCGExFactories::ClusterNodeFilters, false);
		GetInputFactories(Context, PCGExRefineEdges::SourceEdgeFilters, Context->EdgeFilterFactories, PCGExFactories::ClusterEdgeFilters, false);
	}

	if (Settings->Sanitization == EPCGExRefineSanitization::Filters)
	{
		GetInputFactories(Context, PCGExRefineEdges::SourceSanitizeEdgeFilters, Context->SanitizationFilterFactories, PCGExFactories::ClusterEdgeFilters, false);
	}

	return true;
}

bool FPCGExRefineEdgesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExRefineEdgesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(RefineEdges)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (Settings->bOutputOnlyEdgesAsPoints)
		{
			if (!Context->StartProcessingClusters<PCGExClusterMT::TBatch<PCGExRefineEdges::FProcessor>>(
				[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
				[&](PCGExClusterMT::TBatch<PCGExRefineEdges::FProcessor>* NewBatch)
				{
					if (Context->Refinement->RequiresHeuristics()) { NewBatch->SetRequiresHeuristics(true); }
				},
				PCGExMT::State_Done))
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
				return true;
			}
		}
		else
		{
			if (!Context->StartProcessingClusters<PCGExClusterMT::TBatchWithGraphBuilder<PCGExRefineEdges::FProcessor>>(
				[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
				[&](PCGExClusterMT::TBatchWithGraphBuilder<PCGExRefineEdges::FProcessor>* NewBatch)
				{
					NewBatch->GraphBuilderDetails = Context->GraphBuilderDetails;
					if (Context->Refinement->RequiresHeuristics()) { NewBatch->SetRequiresHeuristics(true); }
				},
				PCGExMT::State_Done))
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));

				return true;
			}
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	if (!Settings->bOutputOnlyEdgesAsPoints) { Context->MainPoints->OutputToContext(); }
	else { Context->MainEdges->OutputToContext(); }

	return Context->TryComplete();
}

namespace PCGExRefineEdges
{
	PCGExCluster::FCluster* FProcessor::HandleCachedCluster(const PCGExCluster::FCluster* InClusterRef)
	{
		// Create a light working copy with edges only, will be deleted.
		bDeleteCluster = true;
		return new PCGExCluster::FCluster(InClusterRef, VtxIO, EdgesIO, false, true, false);
	}

	FProcessor::~FProcessor()
	{
		PCGEX_DELETE_OPERATION(Refinement)
		PCGEX_DELETE(EdgeFilterManager);
		PCGEX_DELETE(SanitizationFilterManager);
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExRefineEdges::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(RefineEdges)

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		LocalSettings = Settings;
		LocalTypedContext = TypedContext;

		Sanitization = Settings->Sanitization;

		Refinement = TypedContext->Refinement->CopyOperation<UPCGExEdgeRefineOperation>();
		Refinement->PrepareForCluster(Cluster, HeuristicsHandler);

		Refinement->EdgesFilters = &EdgeFilterCache;
		EdgeFilterCache.Init(true, EdgeDataFacade->Source->GetNum());

		if (!TypedContext->EdgeFilterFactories.IsEmpty())
		{
			EdgeFilterManager = new PCGExPointFilter::TManager(EdgeDataFacade);
			if (!EdgeFilterManager->Init(Context, TypedContext->EdgeFilterFactories)) { return false; }
		}
		else
		{
		}

		const int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetClusterBatchChunkSize();

		if (Settings->Sanitization == EPCGExRefineSanitization::Filters)
		{
			if (!TypedContext->SanitizationFilterFactories.IsEmpty())
			{
				SanitizationFilterManager = new PCGExPointFilter::TManager(EdgeDataFacade);
				if (!SanitizationFilterManager->Init(Context, TypedContext->SanitizationFilterFactories)) { return false; }
			}
		}

		// Need to go through PrepareSingleLoopScopeForEdges anyway

		if (Refinement->RequiresIndividualEdgeProcessing())
		{
			StartParallelLoopForEdges();
		}
		else
		{
			PCGEX_ASYNC_GROUP(AsyncManagerPtr, EdgeScopeLoop)
			EdgeScopeLoop->SetOnCompleteCallback(
				[&]()
				{
					if (Refinement->RequiresIndividualNodeProcessing()) { StartParallelLoopForNodes(); }
					else { Refinement->Process(); }
				});
			EdgeScopeLoop->SetOnIterationRangeStartCallback(
				[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx) { PrepareSingleLoopScopeForEdges(StartIndex, Count); });
			EdgeScopeLoop->PrepareRangesOnly(EdgesIO->GetNum(), PLI);
		}

		return true;
	}

	void FProcessor::ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const int32 LoopIdx, const int32 Count)
	{
		Refinement->ProcessNode(Node);
	}

	void FProcessor::PrepareSingleLoopScopeForEdges(const uint32 StartIndex, const int32 Count)
	{
		const int32 MaxIndex = StartIndex + Count;

		EdgeDataFacade->Fetch(StartIndex, Count);

		TArray<PCGExGraph::FIndexedEdge>& Edges = *Cluster->Edges;

		const bool bDefaultValidity = Refinement->GetDefaultEdgeValidity();

		if (EdgeFilterManager)
		{
			for (int i = StartIndex; i < MaxIndex; i++)
			{
				EdgeFilterCache[i] = EdgeFilterManager->Test(i);
				Edges[i].bValid = bDefaultValidity;
			}
		}
		else
		{
			for (int i = StartIndex; i < MaxIndex; i++)
			{
				Edges[i].bValid = bDefaultValidity;
			}
		}
	}

	void FProcessor::ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FIndexedEdge& Edge, const int32 LoopIdx, const int32 Count)
	{
		Refinement->ProcessEdge(Edge);
	}

	void FProcessor::Sanitize()
	{
		Cluster->GetExpandedEdges(true); //Oof

		PCGEX_ASYNC_GROUP(AsyncManagerPtr, SanitizeTaskGroup)
		SanitizeTaskGroup->SetOnCompleteCallback([&]() { InsertEdges(); });

		if (LocalSettings->Sanitization == EPCGExRefineSanitization::Filters)
		{
			const int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetClusterBatchChunkSize();
			SanitizeTaskGroup->SetOnIterationRangeStartCallback(
				[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
				{
					const int32 MaxIndex = StartIndex + Count;
					for (int i = StartIndex; i < MaxIndex; i++) { if (SanitizationFilterManager->Test(i)) { (Cluster->Edges->GetData() + i)->bValid = true; } }
				});
			SanitizeTaskGroup->PrepareRangesOnly(EdgesIO->GetNum(), PLI);
		}
		else
		{
			SanitizeTaskGroup->StartRanges<FSanitizeRangeTask>(
				NumNodes, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize(),
				nullptr, this);
		}
	}

	void FProcessor::InsertEdges() const
	{
		TArray<PCGExGraph::FIndexedEdge> ValidEdges;
		Cluster->GetValidEdges(ValidEdges);

		if (ValidEdges.IsEmpty()) { return; }

		if (GraphBuilder)
		{
			GraphBuilder->Graph->InsertEdges(ValidEdges);
			return;
		}

		EdgesIO->InitializeOutput<UPCGPointData>(PCGExData::EInit::NewOutput); // Downgrade to regular data
		const TArray<FPCGPoint>& OriginalEdges = EdgesIO->GetIn()->GetPoints();
		TArray<FPCGPoint>& MutableEdges = EdgesIO->GetOut()->GetMutablePoints();
		PCGEX_SET_NUM_UNINITIALIZED(MutableEdges, ValidEdges.Num())
		for (int i = 0; i < ValidEdges.Num(); ++i) { MutableEdges[i] = OriginalEdges[ValidEdges[i].EdgeIndex]; }
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(RefineEdges)

		if (Settings->Sanitization != EPCGExRefineSanitization::None)
		{
			Sanitize();
			return;
		}

		InsertEdges();
	}

	bool FSanitizeRangeTask::ExecuteTask()
	{
		const int32 StartIndex = PCGEx::H64A(Scope);
		const int32 NumIterations = PCGEx::H64B(Scope);

		if (Processor->Sanitization == EPCGExRefineSanitization::Longest)
		{
			for (int i = 0; i < NumIterations; ++i)
			{
				const PCGExCluster::FNode& Node = *(Processor->Cluster->Nodes->GetData() + StartIndex + i);

				int32 BestIndex = -1;
				double LongestDist = TNumericLimits<double>::Min();

				for (const uint64 AdjacencyHash : Node.Adjacency)
				{
					uint32 OtherNodeIndex;
					uint32 EdgeIndex;
					PCGEx::H64(AdjacencyHash, OtherNodeIndex, EdgeIndex);

					const double Dist = Processor->Cluster->GetDistSquared(Node.NodeIndex, OtherNodeIndex);
					if (Dist > LongestDist)
					{
						LongestDist = Dist;
						BestIndex = EdgeIndex;
					}
				}

				if (BestIndex == -1) { continue; }

				FPlatformAtomics::InterlockedExchange(&(Processor->Cluster->Edges->GetData() + BestIndex)->bValid, 1);
			}
		}
		else if (Processor->Sanitization == EPCGExRefineSanitization::Shortest)
		{
			for (int i = 0; i < NumIterations; ++i)
			{
				const PCGExCluster::FNode& Node = *(Processor->Cluster->Nodes->GetData() + StartIndex + i);

				int32 BestIndex = -1;
				double ShortestDist = TNumericLimits<double>::Max();

				for (const uint64 AdjacencyHash : Node.Adjacency)
				{
					uint32 OtherNodeIndex;
					uint32 EdgeIndex;
					PCGEx::H64(AdjacencyHash, OtherNodeIndex, EdgeIndex);

					const double Dist = Processor->Cluster->GetDistSquared(Node.NodeIndex, OtherNodeIndex);
					if (Dist < ShortestDist)
					{
						ShortestDist = Dist;
						BestIndex = EdgeIndex;
					}
				}

				if (BestIndex == -1) { continue; }

				FPlatformAtomics::InterlockedExchange(&(Processor->Cluster->Edges->GetData() + BestIndex)->bValid, 1);
			}
		}

		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
