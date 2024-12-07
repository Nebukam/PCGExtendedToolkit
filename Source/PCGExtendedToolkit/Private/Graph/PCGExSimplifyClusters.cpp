// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExSimplifyClusters.h"


#include "Graph/PCGExChain.h"
#include "Graph/Filters/PCGExClusterFilter.h"


#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EIOInit UPCGExSimplifyClustersSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::New; }
PCGExData::EIOInit UPCGExSimplifyClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::None; }

#pragma endregion

PCGEX_INITIALIZE_ELEMENT(SimplifyClusters)

bool FPCGExSimplifyClustersElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SimplifyClusters)

	PCGEX_FWD(GraphBuilderDetails)

	return true;
}

bool FPCGExSimplifyClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSimplifyClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SimplifyClusters)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExSimplifyClusters::FBatch>(
			[&](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExSimplifyClusters::FBatch>& NewBatch)
			{
			}))
		{
			Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExGraph::State_ReadyToCompile)
	if (!Context->CompileGraphBuilders(true, PCGEx::State_Done)) { return false; }
	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExSimplifyClusters
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSimplifyClusters::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		ChainBuilder = MakeShared<PCGExCluster::FNodeChainBuilder>(Cluster.ToSharedRef());
		ChainBuilder->Breakpoints = Breakpoints;
		bIsProcessorValid = ChainBuilder->Compile(AsyncManager);

		return true;
	}


	void FProcessor::CompleteWork()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSimplifyClusters::FProcessor::CompleteWork);
		StartParallelLoopForRange(ChainBuilder->Chains.Num());
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope)
	{
		const TSharedPtr<PCGExCluster::FNodeChain> Chain = ChainBuilder->Chains[Iteration];
		if (!Chain) { return; }

		if (Settings->bPruneLeaves && Chain->bIsLeaf) { return; } // Skip leaf

		const bool bComputeMeta = Settings->EdgeUnionData.WriteAny();

		if (Settings->bOperateOnLeavesOnly && !Chain->bIsLeaf)
		{
			Chain->Dump(Cluster.ToSharedRef(), GraphBuilder->Graph, bComputeMeta);
			return;
		}

		if (Chain->SingleEdge != -1 || !Settings->bMergeAboveAngularThreshold)
		{
			Chain->DumpReduced(Cluster.ToSharedRef(), GraphBuilder->Graph, bComputeMeta);
			return;
		}

		const double DotThreshold = PCGExMath::DegreesToDot(Settings->AngularThreshold);
		const int32 IOIndex = EdgeDataFacade->Source->IOIndex;

		PCGExGraph::FEdge OutEdge = PCGExGraph::FEdge{};

		const TArray<PCGExGraph::FLink>& Links = Chain->Links;

		int32 LastIndex = Chain->Seed.Node;
		int32 UnionCount = 0;

		const int32 MaxIndex = Links.Num() - 1;
		const int32 NumIterations = Chain->bIsClosedLoop ? Links.Num() : MaxIndex;

		for (int i = 1; i < NumIterations; ++i)
		{
			UnionCount++;

			const PCGExGraph::FLink Lk = Links[i];

			const FVector A = Cluster->GetDir(Links[i - 1].Node, Lk.Node);
			const FVector B = Cluster->GetDir(Lk.Node, Links[(i == MaxIndex && Chain->bIsClosedLoop) ? 0 : i + 1].Node);

			if (!Settings->bInvertAngularThreshold) { if (FVector::DotProduct(A, B) > DotThreshold) { continue; } }
			else { if (FVector::DotProduct(A, B) < DotThreshold) { continue; } }

			GraphBuilder->Graph->InsertEdge(
				Cluster->GetNode(LastIndex)->PointIndex,
				Cluster->GetNode(Lk)->PointIndex,
				OutEdge, IOIndex);

			if (bComputeMeta)
			{
				// TODO : Compute UnionData to carry over attributes & properties
				GraphBuilder->Graph->GetOrCreateEdgeMetadata(OutEdge.Index).UnionSize = UnionCount;
				UnionCount = 0;
			}

			LastIndex = Lk.Node;
		}

		UnionCount++;
		GraphBuilder->Graph->InsertEdge(
			Cluster->GetNode(LastIndex)->PointIndex,
			Cluster->GetNode(Chain->Links.Last())->PointIndex,
			OutEdge, IOIndex);

		if (bComputeMeta)
		{
			// TODO : Compute UnionData to carry over attributes & properties
			GraphBuilder->Graph->GetOrCreateEdgeMetadata(OutEdge.Index).UnionSize = UnionCount;
		}
	}

	const PCGExGraph::FGraphMetadataDetails* FBatch::GetGraphMetadataDetails()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SimplifyClusters)
		if (!Settings->EdgeUnionData.WriteAny()) { return nullptr; }
		GraphMetadataDetails.Grab(Context, Settings->EdgeUnionData);
		return &GraphMetadataDetails;
	}

	void FBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TBatch<FProcessor>::RegisterBuffersDependencies(FacadePreloader);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SimplifyClusters)
		PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, Context->FilterFactories, FacadePreloader);
	}

	void FBatch::Process()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SimplifyClusters)

		const int32 NumPoints = VtxDataFacade->GetNum();
		Breakpoints = MakeShared<TArray<int8>>();
		Breakpoints->Init(false, NumPoints);

		if (Context->FilterFactories.IsEmpty())
		{
			// Process breakpoint filters
			PCGEX_MAKE_SHARED(FilterManager, PCGExPointFilter::FManager, VtxDataFacade)
			TArray<int8>& Breaks = *Breakpoints;
			if (FilterManager->Init(ExecutionContext, Context->FilterFactories))
			{
				for (int i = 0; i < NumPoints; i++) { Breaks[i] = FilterManager->Test(i); }
			}
		}

		TBatch<FProcessor>::Process();
	}

	bool FBatch::PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor)
	{
		ClusterProcessor->Breakpoints = Breakpoints;
		return TBatch<FProcessor>::PrepareSingle(ClusterProcessor);
	}
}


#undef LOCTEXT_NAMESPACE
