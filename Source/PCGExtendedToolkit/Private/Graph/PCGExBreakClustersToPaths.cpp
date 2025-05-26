// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBreakClustersToPaths.h"

#include "Curve/CurveUtil.h"
#include "Graph/PCGExChain.h"
#include "Graph/Filters/PCGExClusterFilter.h"
#include "Paths/PCGExPaths.h"

#define LOCTEXT_NAMESPACE "PCGExBreakClustersToPaths"
#define PCGEX_NAMESPACE BreakClustersToPaths

TArray<FPCGPinProperties> UPCGExBreakClustersToPathsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExPaths::OutputPathsLabel, "Paths", Required, {})
	return PinProperties;
}

PCGExData::EIOInit UPCGExBreakClustersToPathsSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::None; }
PCGExData::EIOInit UPCGExBreakClustersToPathsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

PCGEX_INITIALIZE_ELEMENT(BreakClustersToPaths)

bool FPCGExBreakClustersToPathsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BreakClustersToPaths)

	Context->Paths = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->Paths->OutputPin = PCGExPaths::OutputPathsLabel;

	return true;
}

bool FPCGExBreakClustersToPathsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBreakClustersToPathsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BreakClustersToPaths)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExBreakClustersToPaths::FBatch>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExBreakClustersToPaths::FBatch>& NewBatch)
			{
				if (Settings->OperateOn == EPCGExBreakClusterOperationTarget::Paths)
				{
					NewBatch->VtxFilterFactories = &Context->FilterFactories;
				}
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)

	Context->Paths->StageOutputs();
	return Context->TryComplete();
}

namespace PCGExBreakClustersToPaths
{
	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBreakClustersToPaths::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		if (!DirectionSettings.InitFromParent(ExecutionContext, GetParentBatch<FBatch>()->DirectionSettings, EdgeDataFacade)) { return false; }

		if (Settings->OperateOn == EPCGExBreakClusterOperationTarget::Paths)
		{
			if (VtxFiltersManager)
			{
				PCGEX_ASYNC_GROUP_CHKD(AsyncManager, FilterBreakpoints)

				FilterBreakpoints->OnCompleteCallback =
					[PCGEX_ASYNC_THIS_CAPTURE]()
					{
						PCGEX_ASYNC_THIS
						This->BuildChains();
					};

				FilterBreakpoints->OnSubLoopStartCallback =
					[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
					{
						PCGEX_ASYNC_THIS
						This->FilterVtxScope(Scope);
					};

				FilterBreakpoints->StartSubLoops(NumNodes, GetDefault<UPCGExGlobalSettings>()->GetClusterBatchChunkSize());
			}
			else
			{
				return BuildChains();
			}
		}
		else
		{
			StartParallelLoopForEdges();
		}

		return true;
	}

	bool FProcessor::BuildChains()
	{
		ChainBuilder = MakeShared<PCGExCluster::FNodeChainBuilder>(Cluster.ToSharedRef());
		ChainBuilder->Breakpoints = VtxFilterCache;
		if (Settings->LeavesHandling == EPCGExBreakClusterLeavesHandling::Only)
		{
			bIsProcessorValid = ChainBuilder->CompileLeavesOnly(AsyncManager);
		}
		else
		{
			bIsProcessorValid = ChainBuilder->Compile(AsyncManager);
		}

		return bIsProcessorValid;
	}


	void FProcessor::CompleteWork()
	{
		if (Settings->OperateOn == EPCGExBreakClusterOperationTarget::Paths)
		{
			if (ChainBuilder->Chains.IsEmpty())
			{
				bIsProcessorValid = false;
				return;
			}

			StartParallelLoopForRange(ChainBuilder->Chains.Num());
		}
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		PCGEX_SCOPE_LOOP(Index)
		{
			const TSharedPtr<PCGExCluster::FNodeChain> Chain = ChainBuilder->Chains[Index];
			if (!Chain) { continue; }

			if (Settings->LeavesHandling == EPCGExBreakClusterLeavesHandling::Exclude && Chain->bIsLeaf) { continue; }

			const int32 ChainSize = Chain->Links.Num() + 1;

			if (ChainSize < Settings->MinPointCount) { continue; }
			if (Settings->bOmitAbovePointCount && ChainSize > Settings->MaxPointCount) { continue; }

			const bool bReverse = DirectionSettings.SortExtrapolation(Cluster.Get(), Chain->Seed.Edge, Chain->Seed.Node, Chain->Links.Last().Node);

			const TSharedPtr<PCGExData::FPointIO> PathIO = Context->Paths->Emplace_GetRef<UPCGPointArrayData>(VtxDataFacade->Source, PCGExData::EIOInit::New);
			if (!PathIO) { continue; }

			bool bDoReverse = bReverse;
			(void)PCGEx::AllocateNumPoints(PathIO->GetOut(), ChainSize);

			TArray<int32> IdxMapping = PathIO->GetIdxMapping();
			IdxMapping[0] = Cluster->GetNodePointIndex(Chain->Seed);

			if (ProjectedPositions && (!Settings->bWindOnlyClosedLoops || Chain->bIsClosedLoop))
			{
				const TArray<FVector2D>& PP = *ProjectedPositions;
				TArray<FVector2D> ProjectedPoints;
				ProjectedPoints.SetNumUninitialized(ChainSize);

				ProjectedPoints[0] = PP[Cluster->GetNodePointIndex(Chain->Seed)];

				for (int i = 1; i < ChainSize; i++)
				{
					const int32 PtIndex = Cluster->GetNodePointIndex(Chain->Links[i - 1]);
					IdxMapping[i] = PtIndex;
					ProjectedPoints[i] = PP[PtIndex];
				}

				if (!PCGExGeo::IsWinded(Settings->Winding, UE::Geometry::CurveUtil::SignedArea2<double, FVector2D>(ProjectedPoints) < 0))
				{
					bDoReverse = true;
				}
			}
			else
			{
				for (int i = 1; i < ChainSize; i++) { IdxMapping[i] = Cluster->GetNodePointIndex(Chain->Links[i - 1]); }
			}

			if (bDoReverse) { Algo::Reverse(IdxMapping); }

			if (!Chain->bIsClosedLoop) { if (Settings->bTagIfOpenPath) { PathIO->Tags->AddRaw(Settings->IsOpenPathTag); } }
			else { if (Settings->bTagIfClosedLoop) { PathIO->Tags->AddRaw(Settings->IsClosedLoopTag); } }

			PathIO->ConsumeIdxMapping(EPCGPointNativeProperties::All);
		}
	}

	void FProcessor::ProcessEdges(const PCGExMT::FScope& Scope)
	{
		TArray<PCGExGraph::FEdge>& ClusterEdges = *Cluster->Edges;

		PCGEX_SCOPE_LOOP(Index)
		{
			PCGExGraph::FEdge& Edge = ClusterEdges[Index];

			const TSharedPtr<PCGExData::FPointIO> PathIO = Context->Paths->Emplace_GetRef<UPCGPointArrayData>(VtxDataFacade->Source, PCGExData::EIOInit::New);
			if (!PathIO) { continue; }

			UPCGBasePointData* MutablePoints = PathIO->GetOut();
			(void)PCGEx::AllocateNumPoints(MutablePoints, 2);

			DirectionSettings.SortEndpoints(Cluster.Get(), Edge);

			TArray<int32> IdxMapping = PathIO->GetIdxMapping();
			IdxMapping[0] = Edge.Start;
			IdxMapping[1] = Edge.End;

			PathIO->ConsumeIdxMapping(EPCGPointNativeProperties::All);
		}
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExBreakClustersToPathsContext, UPCGExBreakClustersToPathsSettings>::Cleanup();
		ChainBuilder.Reset();
	}

	void FBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TBatch<FProcessor>::RegisterBuffersDependencies(FacadePreloader);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BreakClustersToPaths)
		DirectionSettings.RegisterBuffersDependencies(ExecutionContext, FacadePreloader);

		if (Settings->Winding != EPCGExWindingMutation::Unchanged && Settings->ProjectionDetails.bLocalProjectionNormal)
		{
			FacadePreloader.Register<FVector>(Context, Settings->ProjectionDetails.LocalNormal);
		}
	}

	void FBatch::OnProcessingPreparationComplete()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BreakClustersToPaths)

		DirectionSettings = Settings->DirectionSettings;
		if (!DirectionSettings.Init(Context, VtxDataFacade, Context->GetEdgeSortingRules()))
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some vtx are missing the specified Direction attribute."));
			return;
		}

		TBatch<FProcessor>::OnProcessingPreparationComplete();
	}

	void FBatch::Process()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BreakClustersToPaths)

		if (Settings->OperateOn == EPCGExBreakClusterOperationTarget::Edges)
		{
			TBatch<FProcessor>::Process();
			return;
		}

		if (Settings->Winding != EPCGExWindingMutation::Unchanged)
		{
			// We're after a specific winding, project points first.

			ProjectionDetails = Settings->ProjectionDetails;
			if (!ProjectionDetails.Init(Context, VtxDataFacade)) { return; }

			PCGEx::InitArray(ProjectedPositions, VtxDataFacade->GetNum());

			PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, ProjectionTaskGroup)

			ProjectionTaskGroup->OnCompleteCallback =
				[PCGEX_ASYNC_THIS_CAPTURE]()
				{
					PCGEX_ASYNC_THIS
					This->OnProjectionComplete();
				};

			ProjectionTaskGroup->OnSubLoopStartCallback =
				[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
				{
					PCGEX_ASYNC_THIS
					This->ProjectionDetails.ProjectFlat(This->VtxDataFacade, *This->ProjectedPositions, Scope);
				};

			ProjectionTaskGroup->StartSubLoops(VtxDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
		}
		else
		{
			TBatch<FProcessor>::Process();
		}
	}

	void FBatch::OnProjectionComplete()
	{
		TBatch<FProcessor>::Process();
	}

	bool FBatch::PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor)
	{
		ClusterProcessor->ProjectedPositions = ProjectedPositions;
		return TBatch<FProcessor>::PrepareSingle(ClusterProcessor);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
