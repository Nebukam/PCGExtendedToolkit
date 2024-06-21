// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExSimplifyClusters.h"

#include "Data/PCGExGraphDefinition.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExSimplifyClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }
PCGExData::EInit UPCGExSimplifyClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FName UPCGExSimplifyClustersSettings::GetVtxFilterLabel() const { return PCGExDataFilter::SourceFiltersLabel; }

#pragma endregion

FPCGExSimplifyClustersContext::~FPCGExSimplifyClustersContext()
{
	PCGEX_TERMINATE_ASYNC
}

PCGEX_INITIALIZE_ELEMENT(SimplifyClusters)

bool FPCGExSimplifyClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SimplifyClusters)

	PCGEX_FWD(GraphBuilderSettings)
	Context->GraphBuilderSettings.bPruneIsolatedPoints = true;

	return true;
}

bool FPCGExSimplifyClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSimplifyClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SimplifyClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatchWithGraphBuilder<PCGExSimplifyClusters::FProcessor>>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExClusterMT::TBatchWithGraphBuilder<PCGExSimplifyClusters::FProcessor>* NewBatch)
			{
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
		Context->OutputMainPoints();
		Context->ExecuteEnd();
	}

	return Context->IsDone();
}

namespace PCGExSimplifyClusters
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE_TARRAY(Chains)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSimplifyClusters::FProcessor::Process);

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		PCGEX_SETTINGS(SimplifyClusters)

		for (int i = 0; i < Cluster->Nodes.Num(); i++) { if (Cluster->Nodes[i].IsComplex()) { VtxFilterCache[i] = true; } }

		if (IsTrivial())
		{
			AsyncManagerPtr->StartSynchronous<PCGExClusterTask::FFindNodeChains>(
				EdgesIO->IOIndex, nullptr, Cluster,
				&VtxFilterCache, &Chains, false, Settings->bOperateOnDeadEndsOnly);
		}
		else
		{
			AsyncManagerPtr->Start<PCGExClusterTask::FFindNodeChains>(
				EdgesIO->IOIndex, nullptr, Cluster,
				&VtxFilterCache, &Chains, false, Settings->bOperateOnDeadEndsOnly);
		}

		return true;
	}


	void FProcessor::CompleteWork()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSimplifyClusters::FProcessor::CompleteWork);

		PCGEX_SETTINGS(SimplifyClusters)

		PCGExClusterTask::DedupeChains(Chains);
		StartParallelLoopForRange(Chains.Num());
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration)
	{
		PCGEX_SETTINGS(SimplifyClusters)

		PCGExCluster::FNodeChain* Chain = Chains[Iteration];
		if (!Chain) { return; }

		TArray<PCGExCluster::FNode>& Nodes = Cluster->Nodes;
		TArray<PCGExGraph::FIndexedEdge>& Edges = Cluster->Edges;
		const int32 IOIndex = Cluster->EdgesIO->IOIndex;

		const double FixedDotThreshold = PCGExMath::DegreesToDot(Settings->AngularThreshold);

		const bool bIsDeadEnd = Nodes[Chain->First].Adjacency.Num() == 1 || Nodes[Chain->Last].Adjacency.Num() == 1;

		if (Settings->bPruneDeadEnds && bIsDeadEnd) { return; }

		PCGExGraph::FIndexedEdge NewEdge = PCGExGraph::FIndexedEdge{};

		if (!Settings->bMergeAboveAngularThreshold)
		{
			GraphBuilder->Graph->InsertEdge(
				Nodes[Chain->First].PointIndex,
				Nodes[Chain->Last].PointIndex,
				NewEdge, IOIndex);

			return;
		}

		if (Chain->SingleEdge != -1)
		{
			GraphBuilder->Graph->InsertEdge(Edges[Chain->SingleEdge]);
			return;
		}


		int32 StartIndex = Chain->First;
		const int32 LastIndex = Chain->Nodes.Num() - 1;

		TSet<uint64> NewEdges;

		for (int i = 0; i < Chain->Nodes.Num(); i++)
		{
			const int32 CurrentIndex = Chain->Nodes[i];

			PCGExCluster::FNode& CurrentNode = Nodes[CurrentIndex];
			PCGExCluster::FNode& PrevNode = i == 0 ? Nodes[Chain->First] : Nodes[Chain->Nodes[i - 1]];
			PCGExCluster::FNode& NextNode = i == LastIndex ? Nodes[Chain->Last] : Nodes[Chain->Nodes[i + 1]];

			const FVector A = (PrevNode.Position - CurrentNode.Position).GetSafeNormal();
			const FVector B = (CurrentNode.Position - NextNode.Position).GetSafeNormal();

			if (!Settings->bInvertAngularThreshold) { if (FVector::DotProduct(A, B) > FixedDotThreshold) { continue; } }
			else { if (FVector::DotProduct(A, B) < FixedDotThreshold) { continue; } }

			NewEdges.Add(PCGEx::H64U(Nodes[StartIndex].PointIndex, Nodes[CurrentIndex].PointIndex));
			StartIndex = CurrentIndex;
		}

		NewEdges.Add(PCGEx::H64U(Nodes[StartIndex].PointIndex, Nodes[Chain->Last].PointIndex)); // Wrap

		GraphBuilder->Graph->InsertEdges(NewEdges, IOIndex);
	}
}


#undef LOCTEXT_NAMESPACE
