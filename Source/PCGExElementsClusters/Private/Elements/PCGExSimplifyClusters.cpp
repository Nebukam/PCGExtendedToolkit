// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExSimplifyClusters.h"

#include "Clusters/PCGExCluster.h"
#include "Clusters/Artifacts/PCGExChain.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Core/PCGExUnionData.h"
#include "Graphs/PCGExChainHelpers.h"
#include "Graphs/PCGExGraph.h"
#include "Graphs/PCGExGraphBuilder.h"
#include "Graphs/PCGExGraphCommon.h"
#include "Math/PCGExMath.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EIOInit UPCGExSimplifyClustersSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::New; }
PCGExData::EIOInit UPCGExSimplifyClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::NoInit; }

#pragma endregion
TArray<FPCGPinProperties> UPCGExSimplifyClustersSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_FILTERS(PCGExClusters::Labels::SourceEdgeFiltersLabel, "Optional edge filters.", Normal)
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(SimplifyClusters)
PCGEX_ELEMENT_BATCH_EDGE_IMPL_ADV(SimplifyClusters)

bool FPCGExSimplifyClustersElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExClustersProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SimplifyClusters)

	PCGEX_FWD(GraphBuilderDetails)
	PCGEX_FWD(EdgeCarryOverDetails)

	Context->EdgeCarryOverDetails.Init();

	GetInputFactories(Context, PCGExClusters::Labels::SourceEdgeFiltersLabel, Context->EdgeFilterFactories, PCGExFactories::ClusterEdgeFilters, false);

	return true;
}

bool FPCGExSimplifyClustersElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSimplifyClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SimplifyClusters)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters([&](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; }, [&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
		{
		}))
		{
			Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExGraphs::States::State_ReadyToCompile)
	if (!Context->CompileGraphBuilders(true, PCGExCommon::States::State_Done)) { return false; }
	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExSimplifyClusters
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSimplifyClusters::Process);

		if (Settings->bMergeAboveAngularThreshold && Settings->bFuseCollocated)
		{
			FuseDistance = FMath::Square(Settings->FuseDistance);
		}

		EdgeDataFacade->bSupportsScopedGet = true;
		if (!Context->EdgeFilterFactories.IsEmpty()) { EdgeFilterFactories = &Context->EdgeFilterFactories; }

		if (!IProcessor::Process(InTaskManager)) { return false; }

		if (!Context->EdgeFilterFactories.IsEmpty()) { StartParallelLoopForEdges(); }
		else { CompileChains(); }

		return true;
	}

	void FProcessor::ProcessEdges(const PCGExMT::FScope& Scope)
	{
		EdgeDataFacade->Fetch(Scope);
		FilterEdgeScope(Scope);

		TArray<int8>& BreakpointsRef = *Breakpoints.Get();

		if (Settings->EdgeFilterRole == EPCGExSimplifyClusterEdgeFilterRole::Collapse)
		{
			// Collapse endpoints
			PCGEX_SCOPE_LOOP(Index)
			{
				if (!EdgeFilterCache[Index]) { continue; }
				PCGExGraphs::FEdge* Edge = Cluster->GetEdge(Index);
				FPlatformAtomics::InterlockedExchange(&BreakpointsRef[Edge->Start], 0);
				FPlatformAtomics::InterlockedExchange(&BreakpointsRef[Edge->End], 0);
			}
		}
		else
		{
			// Restore endpoints
			PCGEX_SCOPE_LOOP(Index)
			{
				if (!EdgeFilterCache[Index]) { continue; }
				PCGExGraphs::FEdge* Edge = Cluster->GetEdge(Index);
				FPlatformAtomics::InterlockedExchange(&BreakpointsRef[Edge->Start], 1);
				FPlatformAtomics::InterlockedExchange(&BreakpointsRef[Edge->End], 1);
			}
		}
	}

	void FProcessor::OnEdgesProcessingComplete()
	{
		CompileChains();
	}

	void FProcessor::CompileChains()
	{
		ChainBuilder = MakeShared<PCGExClusters::FNodeChainBuilder>(Cluster.ToSharedRef());
		ChainBuilder->Breakpoints = Breakpoints;
		bIsProcessorValid = ChainBuilder->Compile(TaskManager);

		EdgesUnion = GraphBuilder->Graph->EdgesUnion;
	}

	void FProcessor::CompleteWork()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSimplifyClusters::FProcessor::CompleteWork);

		StartParallelLoopForRange(ChainBuilder->Chains.Num());
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		PCGEX_SCOPE_LOOP(Index)
		{
			const TSharedPtr<PCGExClusters::FNodeChain> Chain = ChainBuilder->Chains[Index];
			if (!Chain) { continue; }

			if (Settings->bPruneLeaves && Chain->bIsLeaf) { continue; } // Skip leaf

			const bool bComputeMeta = Settings->EdgeUnionData.WriteAny();

			if (Settings->bOperateOnLeavesOnly && !Chain->bIsLeaf)
			{
				PCGExClusters::ChainHelpers::Dump(Chain.ToSharedRef(), Cluster.ToSharedRef(), GraphBuilder->Graph, bComputeMeta);
				continue;
			}

			if (Chain->SingleEdge != -1 || !Settings->bMergeAboveAngularThreshold)
			{
				// TODO : When using reduced dump we know in advance the number of edges will be the number of chains (optionally minus leaves)
				// We can pre-populate the graph union data
				PCGExClusters::ChainHelpers::DumpReduced(Chain.ToSharedRef(), Cluster.ToSharedRef(), GraphBuilder->Graph, bComputeMeta);
				continue;
			}

			const double DotThreshold = PCGExMath::DegreesToDot(Settings->AngularThreshold);
			const int32 IOIndex = EdgeDataFacade->Source->IOIndex;

			PCGExGraphs::FEdge OutEdge = PCGExGraphs::FEdge{};

			const TArray<PCGExGraphs::FLink>& Links = Chain->Links;

			int32 LastIndex = Chain->Seed.Node;
			int32 UnionCount = 0;

			const int32 MaxIndex = Links.Num() - 1;
			const int32 NumIterations = Links.Num();

			TArray<int32> MergedEdges;
			MergedEdges.Reserve(NumIterations);

			FVector LastPosition = Cluster->GetPos(LastIndex);

			for (int i = 1; i < NumIterations; ++i)
			{
				UnionCount++;

				const PCGExGraphs::FLink Lk = Links[i];
				const FVector A = Cluster->GetDir(Links[i - 1].Node, Lk.Node);
				const int32 IndexB = (i == MaxIndex && Chain->bIsClosedLoop) ? 0 : i + 1;

				if (!Links.IsValidIndex(IndexB)) { continue; }


				const FVector CurrentPosition = Cluster->GetPos(Lk);
				const FVector B = Cluster->GetDir(Lk.Node, Links[IndexB].Node);
				bool bSkip = false;

				if (!Settings->bInvertAngularThreshold) { if (FVector::DotProduct(A, B) > DotThreshold) { bSkip = true; } }
				else { if (FVector::DotProduct(A, B) < DotThreshold) { bSkip = true; } }

				if (!bSkip && FuseDistance > 0) { bSkip = FVector::DistSquared(LastPosition, CurrentPosition) <= FuseDistance; }

				if (bSkip)
				{
					MergedEdges.Add(Lk.Edge);
					continue;
				}

				LastPosition = CurrentPosition;

				GraphBuilder->Graph->InsertEdge(Cluster->GetNodePointIndex(LastIndex), Cluster->GetNodePointIndex(Lk), OutEdge, IOIndex);

				PCGExGraphs::FGraphEdgeMetadata& EdgeMetadata = GraphBuilder->Graph->GetOrCreateEdgeMetadata(OutEdge.Index);
				EdgeMetadata.UnionSize = UnionCount;
				EdgesUnion->NewEntryAt_Unsafe(OutEdge.Index)->Add(IOIndex, MergedEdges);

				UnionCount = 0;
				MergedEdges.Reset();

				LastIndex = Lk.Node;
			}

			auto MakeLastEdge = [&](const PCGExGraphs::FLink Link)
			{
				UnionCount++;

				GraphBuilder->Graph->InsertEdge(Cluster->GetNodePointIndex(LastIndex), Cluster->GetNodePointIndex(Link.Node), OutEdge, IOIndex);

				MergedEdges.Add(Link.Edge);

				PCGExGraphs::FGraphEdgeMetadata& EdgeMetadata = GraphBuilder->Graph->GetOrCreateEdgeMetadata(OutEdge.Index);
				EdgeMetadata.UnionSize = UnionCount;
				EdgesUnion->NewEntryAt_Unsafe(OutEdge.Index)->Add(IOIndex, MergedEdges);

				UnionCount = 0;
				MergedEdges.Reset();
			};

			if (LastIndex != Chain->Links.Last().Node)
			{
				// Last processed point is not the last; likely skipped by angular threshold.
				const int32 LastNode = Chain->Links.Last().Node;
				MakeLastEdge(Chain->Links.Last());
				LastIndex = LastNode; // Update last index
			}

			if (Chain->bIsClosedLoop)
			{
				// Wrap
				MakeLastEdge(Chain->Seed);
			}
		}
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExSimplifyClustersContext, UPCGExSimplifyClustersSettings>::Cleanup();
		ChainBuilder.Reset();
	}

	const PCGExGraphs::FGraphMetadataDetails* FBatch::GetGraphMetadataDetails()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SimplifyClusters)
		GraphMetadataDetails.Update(Context, Settings->EdgeUnionData);
		GraphMetadataDetails.EdgesBlendingDetailsPtr = &Settings->EdgeBlendingDetails;
		GraphMetadataDetails.EdgesCarryOverDetails = &Context->EdgeCarryOverDetails;
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

		GraphBuilder->Graph->EdgesUnion = MakeShared<PCGExData::FUnionMetadata>();
		GraphBuilder->Graph->EdgesUnion->SetNum(PCGExData::PCGExPointIO::GetTotalPointsNum(Edges));
		GraphBuilder->Graph->EdgesUnion->bIsAbstract = false; // Because we have valid edge data

		const int32 NumPoints = VtxDataFacade->GetNum();
		Breakpoints = MakeShared<TArray<int8>>();

		if (!Context->FilterFactories.IsEmpty())
		{
			Breakpoints->Init(false, NumPoints);

			// Process breakpoint filters
			PCGEX_MAKE_SHARED(FilterManager, PCGExPointFilter::FManager, VtxDataFacade)
			TArray<int8>& Breaks = *Breakpoints;
			if (FilterManager->Init(ExecutionContext, Context->FilterFactories))
			{
				for (int i = 0; i < NumPoints; i++) { Breaks[i] = FilterManager->Test(i); }
			}
		}
		else
		{
			if (Context->EdgeFilterFactories.IsEmpty()) { Breakpoints->Init(false, NumPoints); }
			else { Breakpoints->Init(Settings->EdgeFilterRole == EPCGExSimplifyClusterEdgeFilterRole::Collapse, NumPoints); }
		}

		TBatch<FProcessor>::Process();
	}

	bool FBatch::PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor)
	{
		PCGEX_TYPED_PROCESSOR
		TypedProcessor->Breakpoints = Breakpoints;
		return TBatch<FProcessor>::PrepareSingle(InProcessor);
	}
}


#undef LOCTEXT_NAMESPACE
