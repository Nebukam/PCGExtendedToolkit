// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExClusterMT.h"

#include "Data/PCGExDataPreloader.h"
#include "Graph/Filters/PCGExClusterFilter.h"

namespace PCGExClusterMT
{
	TSharedPtr<PCGExCluster::FCluster> IProcessor::HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef)
	{
		return MakeShared<PCGExCluster::FCluster>(
			InClusterRef, VtxDataFacade->Source, EdgeDataFacade->Source, NodeIndexLookup,
			false, false, false);
	}

	void IProcessor::ForwardCluster() const
	{
		if (UPCGExClusterEdgesData* EdgesData = Cast<UPCGExClusterEdgesData>(EdgeDataFacade->GetOut()))
		{
			EdgesData->SetBoundCluster(Cluster);
		}
	}

	IProcessor::IProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
		: VtxDataFacade(InVtxDataFacade), EdgeDataFacade(InEdgeDataFacade)
	{
		PCGEX_LOG_CTR(FClusterProcessor)
	}

	void IProcessor::SetExecutionContext(FPCGExContext* InContext)
	{
		ExecutionContext = InContext;
		WorkPermit = ExecutionContext->GetWorkPermit();
		EdgeDataFacade->bSupportsScopedGet = InContext->bScopedAttributeGet && bAllowEdgesDataFacadeScopedGet;
	}

	void IProcessor::SetProjectionDetails(const FPCGExGeo2DProjectionDetails& InDetails, const TSharedPtr<TArray<FVector2D>>& InProjectedVtxPositions, const bool InWantsProjection)
	{
		ProjectionDetails = InDetails;
		ProjectedVtxPositions = InProjectedVtxPositions;
		bWantsProjection = InWantsProjection;
	}

	void IProcessor::RegisterConsumableAttributesWithFacade() const
	{
		// Gives an opportunity for the processor to register attributes with a valid facade
		// So selectors shortcut can be properly resolved (@Last, etc.)

		if (HeuristicsFactories)
		{
			PCGExFactories::RegisterConsumableAttributesWithFacade(*HeuristicsFactories, VtxDataFacade);
			PCGExFactories::RegisterConsumableAttributesWithFacade(*HeuristicsFactories, EdgeDataFacade);
		}
	}

	void IProcessor::SetWantsHeuristics(const bool bRequired, const TArray<TObjectPtr<const UPCGExHeuristicsFactoryData>>* InHeuristicsFactories)
	{
		HeuristicsFactories = InHeuristicsFactories;
		bWantsHeuristics = bRequired;
	}

	bool IProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
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

		if (ProjectedVtxPositions)
		{
			TArray<FVector2D>& ProjectedVtx = *ProjectedVtxPositions.Get();
			Cluster->ProjectedCentroid = FVector2D::ZeroVector;

			if (bWantsProjection && ProjectionDetails.Method == EPCGExProjectionMethod::BestFit)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(IProcessor::Process::Project);

				const TConstPCGValueRange<FTransform> InVtxTransforms = VtxDataFacade->GetIn()->GetConstTransformValueRange();

				const TUniquePtr<PCGExCluster::FCluster::TConstVtxLookup> IdxLookup = MakeUnique<PCGExCluster::FCluster::TConstVtxLookup>(Cluster);
				TArray<int32> PtIndices;
				IdxLookup->Dump(PtIndices);

				ProjectionDetails.Init(PCGExGeo::FBestFitPlane(InVtxTransforms, PtIndices));

				for (const int32 i : PtIndices)
				{
					FVector2D V = FVector2D(ProjectionDetails.ProjectFlat(InVtxTransforms[i].GetLocation(), i));
					ProjectedVtx[i] = V;
					Cluster->ProjectedCentroid += V;
				}
			}
			else
			{
				const TArray<PCGExCluster::FNode>& NodesRef = *Cluster->Nodes.Get();
				for (const PCGExCluster::FNode& Node : NodesRef) { Cluster->ProjectedCentroid += ProjectedVtx[Node.PointIndex]; }
			}

			Cluster->ProjectedCentroid /= Cluster->Nodes->Num();
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

		if (VtxFilterFactories && !InitVtxFilters(VtxFilterFactories)) { return false; }
		if (EdgeFilterFactories && !InitEdgesFilters(EdgeFilterFactories)) { return false; }

		// Building cluster may have taken a while so let's make sure we're still legit
		return AsyncManager->IsAvailable();
	}

	void IProcessor::StartParallelLoopForNodes(const int32 PerLoopIterations)
	{
		PCGEX_ASYNC_CLUSTER_PROCESSOR_LOOP(
			Nodes, NumNodes,
			PrepareLoopScopesForNodes, ProcessNodes,
			OnNodesProcessingComplete,
			bDaisyChainProcessNodes)
	}

	void IProcessor::PrepareLoopScopesForNodes(const TArray<PCGExMT::FScope>& Loops)
	{
	}

	void IProcessor::ProcessNodes(const PCGExMT::FScope& Scope)
	{
	}

	void IProcessor::OnNodesProcessingComplete()
	{
	}

	void IProcessor::StartParallelLoopForEdges(const int32 PerLoopIterations)
	{
		PCGEX_ASYNC_CLUSTER_PROCESSOR_LOOP(
			Edges, NumEdges,
			PrepareLoopScopesForEdges, ProcessEdges,
			OnEdgesProcessingComplete,
			bDaisyChainProcessEdges)
	}

	void IProcessor::PrepareLoopScopesForEdges(const TArray<PCGExMT::FScope>& Loops)
	{
	}

	void IProcessor::ProcessEdges(const PCGExMT::FScope& Scope)
	{
	}

	void IProcessor::OnEdgesProcessingComplete()
	{
	}

	void IProcessor::StartParallelLoopForRange(const int32 NumIterations, const int32 PerLoopIterations)
	{
		PCGEX_ASYNC_CLUSTER_PROCESSOR_LOOP(
			Ranges, NumIterations,
			PrepareLoopScopesForRanges, ProcessRange,
			OnRangeProcessingComplete,
			bDaisyChainProcessRange)
	}

	void IProcessor::PrepareLoopScopesForRanges(const TArray<PCGExMT::FScope>& Loops)
	{
	}

	void IProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
	}

	void IProcessor::OnRangeProcessingComplete()
	{
	}

	void IProcessor::CompleteWork()
	{
	}

	void IProcessor::Write()
	{
	}

	void IProcessor::Output()
	{
	}

	void IProcessor::Cleanup()
	{
		HeuristicsHandler.Reset();
		VtxFiltersManager.Reset();
		EdgesFiltersManager.Reset();
		bIsProcessorValid = false;
	}

	bool IProcessor::InitVtxFilters(const TArray<TObjectPtr<const UPCGExFilterFactoryData>>* InFilterFactories)
	{
		if (InFilterFactories->IsEmpty()) { return true; }

		VtxFiltersManager = MakeShared<PCGExClusterFilter::FManager>(Cluster.ToSharedRef(), VtxDataFacade, EdgeDataFacade);
		VtxFiltersManager->SetSupportedTypes(&PCGExFactories::ClusterNodeFilters);
		return VtxFiltersManager->Init(ExecutionContext, *InFilterFactories);
	}

	void IProcessor::FilterVtxScope(const PCGExMT::FScope& Scope)
	{
		// Note : Don't forget to prefetch VtxDataFacade buffers
		if (VtxFiltersManager) { VtxFiltersManager->Test(Scope.GetView(*Cluster->Nodes.Get()), VtxFilterCache); }
	}

	bool IProcessor::InitEdgesFilters(const TArray<TObjectPtr<const UPCGExFilterFactoryData>>* InFilterFactories)
	{
		EdgeFilterCache.Init(DefaultEdgeFilterValue, EdgeDataFacade->GetNum());

		if (InFilterFactories->IsEmpty()) { return true; }

		EdgesFiltersManager = MakeShared<PCGExClusterFilter::FManager>(Cluster.ToSharedRef(), VtxDataFacade, EdgeDataFacade);
		EdgesFiltersManager->bUseEdgeAsPrimary = true;
		EdgesFiltersManager->SetSupportedTypes(&PCGExFactories::ClusterEdgeFilters);
		return EdgesFiltersManager->Init(ExecutionContext, *InFilterFactories);
	}

	void IProcessor::FilterEdgeScope(const PCGExMT::FScope& Scope)
	{
		// Note : Don't forget to EdgeDataFacade->FetchScope first

		if (EdgesFiltersManager)
		{
			TArray<PCGExGraph::FEdge>& EdgesRef = *Cluster->Edges.Get();
			EdgesFiltersManager->Test(Scope.GetView(EdgesRef), Scope.GetView(EdgeFilterCache));
		}
	}

	IBatch::IBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
		: ExecutionContext(InContext), WorkPermit(InContext->GetWorkPermit()), VtxDataFacade(MakeShared<PCGExData::FFacade>(InVtx))
	{
		PCGEX_LOG_CTR(IBatch)
		SetExecutionContext(InContext);
		Edges.Append(InEdges);
	}

	void IBatch::SetExecutionContext(FPCGExContext* InContext)
	{
		ExecutionContext = InContext;
		WorkPermit = ExecutionContext->GetWorkPermit();
	}

	void IBatch::SetProjectionDetails(const FPCGExGeo2DProjectionDetails& InProjectionDetails)
	{
		bWantsProjection = true;
		bWantsPerClusterProjection = InProjectionDetails.Method == EPCGExProjectionMethod::BestFit;
		ProjectionDetails = InProjectionDetails;
	}

	void IBatch::PrepareProcessing(const TSharedPtr<PCGExMT::FTaskManager> AsyncManagerPtr, const bool bScopedIndexLookupBuild)
	{
		PCGEX_CHECK_WORK_PERMIT_VOID

		AsyncManager = AsyncManagerPtr;
		VtxDataFacade->bSupportsScopedGet = bAllowVtxDataFacadeScopedGet && ExecutionContext->bScopedAttributeGet;

		const int32 NumVtx = VtxDataFacade->GetNum();

		AllocateVtxPoints();

		if (WantsProjection())
		{
			if (ProjectionDetails.Method == EPCGExProjectionMethod::Normal) { ProjectionDetails.Init(VtxDataFacade); }
			else if (!WantsPerClusterProjection()) { ProjectionDetails.Init(PCGExGeo::FBestFitPlane(VtxDataFacade->GetIn()->GetConstTransformValueRange())); }
		}

		if (!bScopedIndexLookupBuild || NumVtx < GetDefault<UPCGExGlobalSettings>()->SmallClusterSize)
		{
			// Trivial
			PCGExGraph::BuildEndpointsLookup(VtxDataFacade->Source, EndpointsLookup, ExpectedAdjacency);
			if (RequiresGraphBuilder())
			{
				GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(VtxDataFacade, &GraphBuilderDetails);
				GraphBuilder->SourceEdgeFacades = EdgesDataFacades;
			}

			if (WantsProjection() && !WantsPerClusterProjection())
			{
				// Prepare projection early, as we want all points projected from the batch				
				PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManagerPtr, ProjectTask)

				ProjectTask->OnCompleteCallback =
					[PCGEX_ASYNC_THIS_CAPTURE]()
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(IBatch::Projection::Complete);
						PCGEX_ASYNC_THIS
						This->OnProcessingPreparationComplete();
					};

				ProjectTask->OnSubLoopStartCallback =
					[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(IBatch::Projection::Range);

						PCGEX_ASYNC_THIS

						const TConstPCGValueRange<FTransform> InVtxTransforms = This->VtxDataFacade->GetIn()->GetConstTransformValueRange();
						const FPCGExGeo2DProjectionDetails& Projection = This->ProjectionDetails;
						TArray<FVector2D>& Proj = *This->ProjectedVtxPositions.Get();

						PCGEX_SCOPE_LOOP(i)
						{
							Proj[i] = FVector2D(Projection.ProjectFlat(InVtxTransforms[i].GetLocation(), i));
						}
					};

				ProjectTask->StartSubLoops(VtxDataFacade->GetNum(), 4096);
			}
			else
			{
				OnProcessingPreparationComplete();
			}
		}
		else
		{
			PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManagerPtr, BuildEndpointLookupTask)

			PCGEx::InitArray(ReverseLookup, NumVtx);
			PCGEx::InitArray(ExpectedAdjacency, NumVtx);

			RawLookupAttribute = PCGEx::TryGetConstAttribute<int64>(VtxDataFacade->GetIn(), PCGExGraph::Attr_PCGExVtxIdx);
			if (!RawLookupAttribute) { return; } // FAIL

			BuildEndpointLookupTask->OnCompleteCallback =
				[PCGEX_ASYNC_THIS_CAPTURE]()
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(IBatch::BuildLookupTable::Complete);

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
					TRACE_CPUPROFILER_EVENT_SCOPE(IBatch::BuildLookupTable::Range);

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

					if (This->WantsProjection() && !This->WantsPerClusterProjection())
					{
						// Extra loop for projection when desired

						const TConstPCGValueRange<FTransform> InVtxTransforms = This->VtxDataFacade->GetIn()->GetConstTransformValueRange();
						const FPCGExGeo2DProjectionDetails& Projection = This->ProjectionDetails;
						TArray<FVector2D>& Proj = *This->ProjectedVtxPositions.Get();

						PCGEX_SCOPE_LOOP(i)
						{
							Proj[i] = FVector2D(Projection.ProjectFlat(InVtxTransforms[i].GetLocation(), i));
						}
					}
				};

			BuildEndpointLookupTask->StartSubLoops(VtxDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
		}
	}

	void IBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		if (VtxFilterFactories)
		{
			PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, *VtxFilterFactories, FacadePreloader);
		}

		// TODO : Preload heuristics that depends on Vtx metadata
	}

	void IBatch::OnProcessingPreparationComplete()
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

	void IBatch::Process()
	{
		bIsBatchValid = false;

		PCGEX_ASYNC_CHKD_VOID(AsyncManager)

		if (VtxDataFacade->GetNum() <= 1) { return; }
		if (VtxFilterFactories)
		{
			VtxFilterCache = MakeShared<TArray<int8>>();
			VtxFilterCache->Init(DefaultVtxFilterValue, VtxDataFacade->GetNum());
		}

		bIsBatchValid = true;
	}

	void IBatch::OnInitialPostProcess()
	{
	}

	void IBatch::CompleteWork()
	{
	}

	void IBatch::Write()
	{
		PCGEX_CHECK_WORK_PERMIT_VOID
		if (bWriteVtxDataFacade && bIsBatchValid) { VtxDataFacade->WriteFastest(AsyncManager); }
	}

	const PCGExGraph::FGraphMetadataDetails* IBatch::GetGraphMetadataDetails()
	{
		return nullptr;
	}

	void IBatch::CompileGraphBuilder(const bool bOutputToContext)
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

	void IBatch::Output()
	{
	}

	void IBatch::Cleanup()
	{
	}

	void IBatch::InternalInitProcessor(const TSharedPtr<IProcessor>& InProcessor, const int32 InIndex)
	{
		InProcessor->SetExecutionContext(ExecutionContext);

		InProcessor->ParentBatch = SharedThis(this);
		InProcessor->VtxFilterFactories = VtxFilterFactories;
		InProcessor->EdgeFilterFactories = EdgeFilterFactories;
		InProcessor->VtxFilterCache = VtxFilterCache;

		InProcessor->NodeIndexLookup = NodeIndexLookup;
		InProcessor->EndpointsLookup = &EndpointsLookup;
		InProcessor->ExpectedAdjacency = &ExpectedAdjacency;
		InProcessor->BatchIndex = InIndex;

		if (WantsProjection()) { InProcessor->SetProjectionDetails(ProjectionDetails, ProjectedVtxPositions, WantsPerClusterProjection()); }

		if (RequiresGraphBuilder()) { InProcessor->GraphBuilder = GraphBuilder; }

		InProcessor->SetWantsHeuristics(WantsHeuristics(), HeuristicsFactories);

		InProcessor->RegisterConsumableAttributesWithFacade();
	}

	void IBatch::AllocateVtxPoints()
	{
		NodeIndexLookup = MakeShared<PCGEx::FIndexLookup>(VtxDataFacade->GetNum());

		if (WantsProjection())
		{
			ProjectedVtxPositions = MakeShared<TArray<FVector2D>>();
			ProjectedVtxPositions->SetNumUninitialized(VtxDataFacade->GetNum());
		}

		if (AllocateVtxProperties == EPCGPointNativeProperties::None) { return; }
		if (VtxDataFacade->GetOut() && VtxDataFacade->GetIn() != VtxDataFacade->GetOut())
		{
			VtxDataFacade->GetOut()->AllocateProperties(AllocateVtxProperties);
		}
	}
}
