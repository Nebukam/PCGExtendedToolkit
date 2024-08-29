// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExRefineEdges.h"

#include "Graph/PCGExGraph.h"
#include "Graph/Edges/Refining/PCGExEdgeRefinePrimMST.h"

#define LOCTEXT_NAMESPACE "PCGExRefineEdges"
#define PCGEX_NAMESPACE RefineEdges

TArray<FPCGPinProperties> UPCGExRefineEdgesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (Refinement && Refinement->RequiresHeuristics()) { PCGEX_PIN_PARAMS(PCGExGraph::SourceHeuristicsLabel, "Heuristics may be required by some refinements.", Required, {}) }
	PCGEX_PIN_PARAMS(PCGExRefineEdges::SourceProtectEdgeFilters, "Filters used to preserve specific edges.", Advanced, {})
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExRefineEdgesSettings::OutputPinProperties() const
{
	if (!bOutputOnlyEdgesAsPoints) { return Super::OutputPinProperties(); }
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Edges but as simple points.", Required, {})
	return PinProperties;
}

PCGExData::EInit UPCGExRefineEdgesSettings::GetMainOutputInitMode() const { return bOutputOnlyEdgesAsPoints ? PCGExData::EInit::NoOutput : GraphBuilderDetails.bPruneIsolatedPoints ? PCGExData::EInit::NewOutput : PCGExData::EInit::DuplicateInput; }
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

	PCGEX_OPERATION_BIND(Refinement, UPCGExEdgeRefinePrimMST)
	PCGEX_FWD(GraphBuilderDetails)

	if (Context->Refinement->RequiresHeuristics() && !Context->bHasValidHeuristics)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("The selected refinement requires heuristics to be connected, but none can be found."));
		return false;
	}

	GetInputFactories(Context, PCGExRefineEdges::SourceProtectEdgeFilters, Context->PreserveEdgeFilterFactories, PCGExFactories::PointFilters, false);

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
		PCGEX_DELETE(FilterManager);
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExRefineEdges::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(RefineEdges)

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		Sanitization = Settings->Sanitization;

		Refinement = TypedContext->Refinement->CopyOperation<UPCGExEdgeRefineOperation>();
		Refinement->PrepareForCluster(Cluster, HeuristicsHandler);

		if (Refinement->InvalidateAllEdgesBeforeProcessing())
		{
			for (PCGExGraph::FIndexedEdge& Edge : *Cluster->Edges) { Edge.bValid = false; }
		}

		if (Refinement->RequiresIndividualNodeProcessing()) { StartParallelLoopForNodes(); }
		else if (Refinement->RequiresIndividualEdgeProcessing()) { StartParallelLoopForEdges(); }
		else { Refinement->Process(); }
		return true;
	}

	void FProcessor::ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node) { Refinement->ProcessNode(Node); }

	void FProcessor::ProcessSingleEdge(PCGExGraph::FIndexedEdge& Edge) { Refinement->ProcessEdge(Edge); }

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
		for (int i = 0; i < ValidEdges.Num(); i++) { MutableEdges[i] = OriginalEdges[ValidEdges[i].EdgeIndex]; }
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(RefineEdges)

		if (!TypedContext->PreserveEdgeFilterFactories.IsEmpty())
		{
			FilterTaskGroup = AsyncManagerPtr->CreateGroup();
			FilterTaskGroup->SetOnCompleteCallback(
				[&]()
				{
					if (Settings->Sanitization == EPCGExRefineSanitization::None)
					{
						InsertEdges();
						return;
					}

					Cluster->GetExpandedEdges(true); //Oof

					SanitizeTaskGroup = AsyncManagerPtr->CreateGroup();
					SanitizeTaskGroup->SetOnCompleteCallback([&]() { InsertEdges(); });
					SanitizeTaskGroup->StartRanges<FSanitizeRangeTask>(
						NumNodes, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize(),
						nullptr, this);
				});

			FilterManager = new PCGExPointFilter::TManager(EdgeDataFacade);
			FilterManager->Init(Context, TypedContext->PreserveEdgeFilterFactories);
			FilterTaskGroup->StartRanges<FFilterRangeTask>(
				NumEdges, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize(),
				nullptr, this);
		}
		else
		{
			InsertEdges();
		}
	}

	bool FFilterRangeTask::ExecuteTask()
	{
		const int32 StartIndex = PCGEx::H64A(Scope);
		const int32 NumIterations = PCGEx::H64B(Scope);

		for (int i = 0; i < NumIterations; i++)
		{
			PCGExGraph::FIndexedEdge& Edge = *(Processor->Cluster->Edges->GetData() + StartIndex + i);
			if (!Edge.bValid) { Edge.bValid = Processor->FilterManager->Test(Edge.EdgeIndex); }
		}

		return true;
	}

	bool FSanitizeRangeTask::ExecuteTask()
	{
		const int32 StartIndex = PCGEx::H64A(Scope);
		const int32 NumIterations = PCGEx::H64B(Scope);

		for (int i = 0; i < NumIterations; i++)
		{
			const PCGExCluster::FNode& Node = *(Processor->Cluster->Nodes->GetData() + StartIndex + i);
			const int32 NumNeighbors = Node.Adjacency.Num();

			if (Processor->Sanitization == EPCGExRefineSanitization::Longest)
			{
				int32 LongestEdge = -1;
				double LongestLength = TNumericLimits<double>::Min();

				for (int h = 0; h < NumNeighbors; h++)
				{
					const PCGExCluster::FExpandedEdge* EEdge = *(Processor->Cluster->ExpandedEdges->GetData() + PCGEx::H64B(Node.Adjacency[h]));
					if (((Processor->Cluster->Edges->GetData() + EEdge->Index))->bValid)
					{
						LongestEdge = -1;
						break;
					}

					const double ELengthSqr = EEdge->GetEdgeLengthSquared(Processor->Cluster);
					if (LongestLength < ELengthSqr)
					{
						LongestLength = ELengthSqr;
						LongestEdge = EEdge->Index;
					}
				}

				if (LongestEdge != -1) { ((Processor->Cluster->Edges->GetData() + LongestEdge))->bValid = true; }
			}
			else
			{
				int32 ShortestEdge = -1;
				double ShortestLength = TNumericLimits<double>::Max();

				for (int h = 0; h < NumNeighbors; h++)
				{
					const PCGExCluster::FExpandedEdge* EEdge = *(Processor->Cluster->ExpandedEdges->GetData() + PCGEx::H64B(Node.Adjacency[h]));
					if (((Processor->Cluster->Edges->GetData() + EEdge->Index))->bValid)
					{
						ShortestEdge = -1;
						break;
					}

					const double ELengthSqr = EEdge->GetEdgeLengthSquared(Processor->Cluster);
					if (ShortestLength > ELengthSqr)
					{
						ShortestLength = ELengthSqr;
						ShortestEdge = EEdge->Index;
					}
				}

				if (ShortestEdge != -1) { ((Processor->Cluster->Edges->GetData() + ShortestEdge))->bValid = true; }
			}
		}

		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
