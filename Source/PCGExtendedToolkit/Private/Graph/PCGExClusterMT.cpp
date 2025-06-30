// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExClusterMT.h"

#include "Graph/Filters/PCGExClusterFilter.h"

namespace PCGExClusterMT
{
	TSharedPtr<PCGExCluster::FCluster> FClusterProcessor::HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef)
	{
		return MakeShared<PCGExCluster::FCluster>(
			InClusterRef, VtxDataFacade->Source, EdgeDataFacade->Source, NodeIndexLookup,
			false, false, false);
	}

	void FClusterProcessor::ForwardCluster() const
	{
		if (UPCGExClusterEdgesData* EdgesData = Cast<UPCGExClusterEdgesData>(EdgeDataFacade->GetOut()))
		{
			EdgesData->SetBoundCluster(Cluster);
		}
	}

	FClusterProcessor::FClusterProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
		: VtxDataFacade(InVtxDataFacade), EdgeDataFacade(InEdgeDataFacade)
	{
		PCGEX_LOG_CTR(FClusterProcessor)
	}

	void FClusterProcessor::SetExecutionContext(FPCGExContext* InContext)
	{
		ExecutionContext = InContext;
		WorkPermit = ExecutionContext->GetWorkPermit();
		EdgeDataFacade->bSupportsScopedGet = InContext->bScopedAttributeGet && bAllowEdgesDataFacadeScopedGet;
	}

	void FClusterProcessor::RegisterConsumableAttributesWithFacade() const
	{
		// Gives an opportunity for the processor to register attributes with a valid facade
		// So selectors shortcut can be properly resolved (@Last, etc.)

		if (HeuristicsFactories)
		{
			PCGExFactories::RegisterConsumableAttributesWithFacade(*HeuristicsFactories, VtxDataFacade);
			PCGExFactories::RegisterConsumableAttributesWithFacade(*HeuristicsFactories, EdgeDataFacade);
		}
	}

	void FClusterProcessor::SetWantsHeuristics(const bool bRequired, const TArray<TObjectPtr<const UPCGExHeuristicsFactoryData>>* InHeuristicsFactories)
	{
		HeuristicsFactories = InHeuristicsFactories;
		bWantsHeuristics = bRequired;
	}

	bool FClusterProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		AsyncManager = InAsyncManager;
		PCGEX_ASYNC_CHKD(AsyncManager)

		PCGEX_CHECK_WORK_PERMIT(false)

		if (!bBuildCluster) { return true; }

		if (const TSharedPtr<PCGExCluster::FCluster> CachedCluster = PCGExClusterData::TryGetCachedCluster(VtxDataFacade->Source, EdgeDataFacade->Source))
		{
			Cluster = HandleCachedCluster(CachedCluster.ToSharedRef());
		}

		if (!Cluster)
		{
			Cluster = MakeShared<PCGExCluster::FCluster>(VtxDataFacade->Source, EdgeDataFacade->Source, NodeIndexLookup);
			Cluster->bIsOneToOne = bIsOneToOne;

			if (!Cluster->BuildFrom(*EndpointsLookup, ExpectedAdjacency))
			{
				PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("A cluster could not be rebuilt correctly. If you did change the content of vtx/edges collections using non cluster-friendly nodes, make sure to use a 'Sanitize Cluster' to ensure clusters are validated."));
				Cluster.Reset();
				return false;
			}
		}

		NumNodes = Cluster->Nodes->Num();
		NumEdges = Cluster->Edges->Num();

		if (bWantsHeuristics)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FClusterProcessor::Heuristics);
			HeuristicsHandler = MakeShared<PCGExHeuristics::FHeuristicsHandler>(ExecutionContext, VtxDataFacade, EdgeDataFacade, *HeuristicsFactories);

			if (!HeuristicsHandler->IsValidHandler()) { return false; }

			HeuristicsHandler->PrepareForCluster(Cluster);
			HeuristicsHandler->CompleteClusterPreparation();
		}

		if (VtxFilterFactories)
		{
			if (!InitVtxFilters(VtxFilterFactories)) { return false; }
		}

		if (EdgeFilterFactories)
		{
			if (!InitEdgesFilters(EdgeFilterFactories)) { return false; }
		}

		// Building cluster may have taken a while so let's make sure we're still legit
		return AsyncManager->IsAvailable();
	}

	void FClusterProcessor::StartParallelLoopForNodes(const int32 PerLoopIterations)
	{
		PCGEX_ASYNC_CLUSTER_PROCESSOR_LOOP(
			Nodes, NumNodes,
			PrepareLoopScopesForNodes, ProcessNodes,
			OnNodesProcessingComplete,
			bDaisyChainProcessNodes)
	}

	void FClusterProcessor::PrepareLoopScopesForNodes(const TArray<PCGExMT::FScope>& Loops)
	{
	}

	void FClusterProcessor::ProcessNodes(const PCGExMT::FScope& Scope)
	{
	}

	void FClusterProcessor::OnNodesProcessingComplete()
	{
	}

	void FClusterProcessor::StartParallelLoopForEdges(const int32 PerLoopIterations)
	{
		PCGEX_ASYNC_CLUSTER_PROCESSOR_LOOP(
			Edges, NumEdges,
			PrepareLoopScopesForEdges, ProcessEdges,
			OnEdgesProcessingComplete,
			bDaisyChainProcessEdges)
	}

	void FClusterProcessor::PrepareLoopScopesForEdges(const TArray<PCGExMT::FScope>& Loops)
	{
	}

	void FClusterProcessor::ProcessEdges(const PCGExMT::FScope& Scope)
	{
	}

	void FClusterProcessor::OnEdgesProcessingComplete()
	{
	}

	void FClusterProcessor::StartParallelLoopForRange(const int32 NumIterations, const int32 PerLoopIterations)
	{
		PCGEX_ASYNC_CLUSTER_PROCESSOR_LOOP(
			Ranges, NumIterations,
			PrepareLoopScopesForRanges, ProcessRange,
			OnRangeProcessingComplete,
			bDaisyChainProcessRange)
	}

	void FClusterProcessor::PrepareLoopScopesForRanges(const TArray<PCGExMT::FScope>& Loops)
	{
	}

	void FClusterProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
	}

	void FClusterProcessor::OnRangeProcessingComplete()
	{
	}

	void FClusterProcessor::CompleteWork()
	{
	}

	void FClusterProcessor::Write()
	{
	}

	void FClusterProcessor::Output()
	{
	}

	void FClusterProcessor::Cleanup()
	{
		HeuristicsHandler.Reset();
		VtxFiltersManager.Reset();
		EdgesFiltersManager.Reset();
		bIsProcessorValid = false;
	}

	bool FClusterProcessor::InitVtxFilters(const TArray<TObjectPtr<const UPCGExFilterFactoryData>>* InFilterFactories)
	{
		if (InFilterFactories->IsEmpty()) { return true; }

		VtxFiltersManager = MakeShared<PCGExClusterFilter::FManager>(Cluster.ToSharedRef(), VtxDataFacade, EdgeDataFacade);
		VtxFiltersManager->SetSupportedTypes(&PCGExFactories::ClusterNodeFilters);
		return VtxFiltersManager->Init(ExecutionContext, *InFilterFactories);
	}

	void FClusterProcessor::FilterVtxScope(const PCGExMT::FScope& Scope)
	{
		// Note : Don't forget to prefetch VtxDataFacade buffers
		if (VtxFiltersManager) { VtxFiltersManager->Test(Scope.GetView(*Cluster->Nodes.Get()), VtxFilterCache); }
	}

	bool FClusterProcessor::InitEdgesFilters(const TArray<TObjectPtr<const UPCGExFilterFactoryData>>* InFilterFactories)
	{
		EdgeFilterCache.Init(DefaultEdgeFilterValue, EdgeDataFacade->GetNum());

		if (InFilterFactories->IsEmpty()) { return true; }

		EdgesFiltersManager = MakeShared<PCGExClusterFilter::FManager>(Cluster.ToSharedRef(), VtxDataFacade, EdgeDataFacade);
		EdgesFiltersManager->bUseEdgeAsPrimary = true;
		EdgesFiltersManager->SetSupportedTypes(&PCGExFactories::ClusterEdgeFilters);
		return EdgesFiltersManager->Init(ExecutionContext, *InFilterFactories);
	}

	void FClusterProcessor::FilterEdgeScope(const PCGExMT::FScope& Scope)
	{
		// Note : Don't forget to EdgeDataFacade->FetchScope first

		if (EdgesFiltersManager)
		{
			TArray<PCGExGraph::FEdge>& EdgesRef = *Cluster->Edges.Get();
			EdgesFiltersManager->Test(Scope.GetView(EdgesRef), Scope.GetView(EdgeFilterCache));
		}
	}

	IClusterProcessorBatch::IClusterProcessorBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
		: ExecutionContext(InContext), WorkPermit(InContext->GetWorkPermit()), VtxDataFacade(MakeShared<PCGExData::FFacade>(InVtx))
	{
		PCGEX_LOG_CTR(IClusterProcessorBatch)
		SetExecutionContext(InContext);
		Edges.Append(InEdges);
	}

	void IClusterProcessorBatch::SetExecutionContext(FPCGExContext* InContext)
	{
		ExecutionContext = InContext;
		WorkPermit = ExecutionContext->GetWorkPermit();
	}

	void IClusterProcessorBatch::PrepareProcessing(const TSharedPtr<PCGExMT::FTaskManager> AsyncManagerPtr, const bool bScopedIndexLookupBuild)
	{
		PCGEX_CHECK_WORK_PERMIT_VOID

		AsyncManager = AsyncManagerPtr;
		VtxDataFacade->bSupportsScopedGet = bAllowVtxDataFacadeScopedGet && ExecutionContext->bScopedAttributeGet;

		const int32 NumVtx = VtxDataFacade->GetNum();
		NodeIndexLookup = MakeShared<PCGEx::FIndexLookup>(NumVtx);

		AllocateVtxPoints();

		if (!bScopedIndexLookupBuild || NumVtx < GetDefault<UPCGExGlobalSettings>()->SmallClusterSize)
		{
			// Trivial
			PCGExGraph::BuildEndpointsLookup(VtxDataFacade->Source, EndpointsLookup, ExpectedAdjacency);
			if (RequiresGraphBuilder())
			{
				GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(VtxDataFacade, &GraphBuilderDetails);
				GraphBuilder->SourceEdgeFacades = EdgesDataFacades;
			}

			OnProcessingPreparationComplete();
		}
		else
		{
			// Spread
			PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManagerPtr, BuildEndpointLookupTask)

			PCGEx::InitArray(ReverseLookup, NumVtx);
			PCGEx::InitArray(ExpectedAdjacency, NumVtx);

			RawLookupAttribute = PCGEx::TryGetConstAttribute<int64>(VtxDataFacade->GetIn(), PCGExGraph::Attr_PCGExVtxIdx);
			if (!RawLookupAttribute) { return; } // FAIL

			BuildEndpointLookupTask->OnCompleteCallback =
				[PCGEX_ASYNC_THIS_CAPTURE]()
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExGraph::BuildLookupTable::Complete);

					PCGEX_ASYNC_THIS

					const int32 Num = This->VtxDataFacade->GetNum();
					This->EndpointsLookup.Reserve(Num);
					for (int i = 0; i < Num; i++) { This->EndpointsLookup.Add(This->ReverseLookup[i], i); }
					This->ReverseLookup.Empty();

					if (This->RequiresGraphBuilder())
					{
						This->GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(This->VtxDataFacade, &This->GraphBuilderDetails);
						This->GraphBuilder->SourceEdgeFacades = This->EdgesDataFacades;
					}

					This->OnProcessingPreparationComplete();
				};

			BuildEndpointLookupTask->OnSubLoopStartCallback =
				[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExGraph::BuildLookupTable::Range);

					PCGEX_ASYNC_THIS

					const TConstPCGValueRange<int64> MetadataEntries = This->VtxDataFacade->GetIn()->GetConstMetadataEntryValueRange();

					PCGEX_SCOPE_LOOP(i)
					{
						uint32 A;
						uint32 B;
						PCGEx::H64(This->RawLookupAttribute->GetValueFromItemKey(MetadataEntries[i]), A, B);

						This->ReverseLookup[i] = A;
						This->ExpectedAdjacency[i] = B;
					}
				};
			BuildEndpointLookupTask->StartSubLoops(VtxDataFacade->GetNum(), 4096);
		}
	}

	void IClusterProcessorBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		if (VtxFilterFactories)
		{
			PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, *VtxFilterFactories, FacadePreloader);
		}

		// TODO : Preload heuristics that depends on Vtx metadata
	}

	void IClusterProcessorBatch::OnProcessingPreparationComplete()
	{
		PCGEX_CHECK_WORK_PERMIT_OR_VOID(!bIsBatchValid)

		VtxFacadePreloader = MakeShared<PCGExData::FFacadePreloader>(VtxDataFacade);
		RegisterBuffersDependencies(*VtxFacadePreloader);

		VtxFacadePreloader->OnCompleteCallback = [PCGEX_ASYNC_THIS_CAPTURE]
		{
			PCGEX_ASYNC_THIS
			This->Process();
		};

		VtxFacadePreloader->StartLoading(AsyncManager);
	}

	void IClusterProcessorBatch::Process()
	{
	}

	void IClusterProcessorBatch::CompleteWork()
	{
	}

	void IClusterProcessorBatch::Write()
	{
		PCGEX_CHECK_WORK_PERMIT_VOID
		if (bWriteVtxDataFacade && bIsBatchValid) { VtxDataFacade->WriteFastest(AsyncManager); }
	}

	const PCGExGraph::FGraphMetadataDetails* IClusterProcessorBatch::GetGraphMetadataDetails()
	{
		return nullptr;
	}

	void IClusterProcessorBatch::CompileGraphBuilder(const bool bOutputToContext)
	{
		PCGEX_CHECK_WORK_PERMIT_OR_VOID(!GraphBuilder || !bIsBatchValid)

		if (bOutputToContext)
		{
			GraphBuilder->OnCompilationEndCallback =
				[PCGEX_ASYNC_THIS_CAPTURE](const TSharedRef<PCGExGraph::FGraphBuilder>& InBuilder, const bool bSuccess)
				{
					PCGEX_ASYNC_THIS

					if (!bSuccess)
					{
						// TODO : Log error
						return;
					}

					if (TSharedPtr<PCGExData::FPointIOCollection> OutCollection = This->GraphEdgeOutputCollection.Pin()) { InBuilder->MoveEdgesOutputs(OutCollection, This->VtxDataFacade->Source->IOIndex * 100000); }
					else { InBuilder->StageEdgesOutputs(); }
				};
		}

		GraphBuilder->CompileAsync(AsyncManager, true, GetGraphMetadataDetails());
	}

	void IClusterProcessorBatch::Output()
	{
	}

	void IClusterProcessorBatch::Cleanup()
	{
	}

	void IClusterProcessorBatch::AllocateVtxPoints() const
	{
		if (AllocateVtxProperties == EPCGPointNativeProperties::None) { return; }
		if (VtxDataFacade->GetOut() && VtxDataFacade->GetIn() != VtxDataFacade->GetOut())
		{
			VtxDataFacade->GetOut()->AllocateProperties(AllocateVtxProperties);
		}
	}
}
