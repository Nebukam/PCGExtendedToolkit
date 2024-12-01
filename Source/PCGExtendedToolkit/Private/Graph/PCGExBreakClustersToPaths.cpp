// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBreakClustersToPaths.h"


#include "Curve/CurveUtil.h"
#include "Graph/PCGExChain.h"
#include "Graph/Filters/PCGExClusterFilter.h"

#define LOCTEXT_NAMESPACE "PCGExBreakClustersToPaths"
#define PCGEX_NAMESPACE BreakClustersToPaths

TArray<FPCGPinProperties> UPCGExBreakClustersToPathsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExGraph::OutputPathsLabel, "Paths", Required, {})
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
	Context->Paths->OutputPin = PCGExGraph::OutputPathsLabel;

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

		if (!DirectionSettings.InitFromParent(ExecutionContext, StaticCastWeakPtr<FBatch>(ParentBatch).Pin()->DirectionSettings, EdgeDataFacade)) { return false; }

		if (Settings->OperateOn == EPCGExBreakClusterOperationTarget::Paths)
		{
			ChainBuilder = MakeShared<PCGExCluster::FNodeChainBuilder>(Cluster.ToSharedRef());
			ChainBuilder->Breakpoints = Breakpoints;
			if (Settings->LeavesHandling == EPCGExBreakClusterLeavesHandling::Only)
			{
				bIsProcessorValid = ChainBuilder->CompileLeavesOnly(AsyncManager);
			}
			else
			{
				bIsProcessorValid = ChainBuilder->Compile(AsyncManager);
			}
		}
		else
		{
			StartParallelLoopForEdges();
		}

		return true;
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

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 Count)
	{
		const TSharedPtr<PCGExCluster::FNodeChain> Chain = ChainBuilder->Chains[Iteration];
		if (!Chain) { return; }

		if (Settings->LeavesHandling == EPCGExBreakClusterLeavesHandling::Exclude && Chain->bIsLeaf) { return; }

		const int32 ChainSize = Chain->Links.Num() + 1;

		if (ChainSize < Settings->MinPointCount) { return; }
		if (Settings->bOmitAbovePointCount && ChainSize > Settings->MaxPointCount) { return; }

		PCGExGraph::FEdge ChainDir = PCGExGraph::FEdge(Chain->Seed.Edge, Cluster->GetNode(Chain->Seed)->PointIndex, Cluster->GetNode(Chain->Links.Last())->PointIndex);
		const bool bReverse = DirectionSettings.SortEndpoints(Cluster.Get(), ChainDir);

		const TSharedPtr<PCGExData::FPointIO> PathIO = Context->Paths->Emplace_GetRef<UPCGPointData>(VtxDataFacade->Source, PCGExData::EIOInit::New);

		TArray<FPCGPoint>& MutablePoints = PathIO->GetOut()->GetMutablePoints();
		MutablePoints.SetNumUninitialized(ChainSize);

		int32 PtIndex = 0;

		MutablePoints[PtIndex++] = PathIO->GetInPoint(Cluster->GetNode(Chain->Seed)->PointIndex);
		for (const PCGExGraph::FLink& Lk : Chain->Links) { MutablePoints[PtIndex++] = PathIO->GetInPoint(Cluster->GetNode(Lk)->PointIndex); }

		if (!Chain->bIsClosedLoop) { if (Settings->bTagIfOpenPath) { PathIO->Tags->Add(Settings->IsOpenPathTag); } }
		else { if (Settings->bTagIfClosedLoop) { PathIO->Tags->Add(Settings->IsClosedLoopTag); } }

		if (bReverse) { Algo::Reverse(MutablePoints); }

		if (ProjectedPositions)
		{
			// TODO : Reverse once only

			if (!Settings->bWindOnlyClosedLoops || Chain->bIsClosedLoop)
			{
				const TArray<FVector2D>& PP = *ProjectedPositions;
				TArray<FVector2D> WindingPoints;
				PCGEx::InitArray(WindingPoints, PtIndex);
				WindingPoints[0] = PP[Cluster->GetNode(Chain->Seed)->PointIndex];
				for (int i = 0; i < PtIndex; i++) { WindingPoints[i + 1] = PP[Cluster->GetNode(Chain->Links[i])->PointIndex]; }

				if (!PCGExGeo::IsWinded(Settings->Winding, UE::Geometry::CurveUtil::SignedArea2<double, FVector2D>(WindingPoints) < 0)
					&& !bReverse)
				{
					Algo::Reverse(MutablePoints);
				}
			}
		}
	}

	void FProcessor::ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FEdge& Edge, const int32 LoopIdx, const int32 Count)
	{
		const TSharedPtr<PCGExData::FPointIO> PathIO = Context->Paths->Emplace_GetRef<UPCGPointData>(VtxDataFacade->Source, PCGExData::EIOInit::New);
		TArray<FPCGPoint>& MutablePoints = PathIO->GetOut()->GetMutablePoints();
		MutablePoints.SetNumUninitialized(2);

		DirectionSettings.SortEndpoints(Cluster.Get(), Edge);

		MutablePoints[0] = PathIO->GetInPoint(Edge.Start);
		MutablePoints[1] = PathIO->GetInPoint(Edge.End);
	}

	void FBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TBatch<FProcessor>::RegisterBuffersDependencies(FacadePreloader);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BreakClustersToPaths)
		PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, Context->FilterFactories, FacadePreloader);
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

		const int32 NumPoints = VtxDataFacade->GetNum();
		Breakpoints = MakeShared<TArray<int8>>();
		Breakpoints->Init(false, NumPoints);

		if (Context->FilterFactories.IsEmpty() || Settings->Winding != EPCGExWindingMutation::Unchanged)
		{
			if (!Context->FilterFactories.IsEmpty())
			{
				// Process breakpoint filters
				BreakpointFilterManager = MakeShared<PCGExPointFilter::FManager>(VtxDataFacade);
				if (!BreakpointFilterManager->Init(ExecutionContext, Context->FilterFactories)) { return; }
			}

			if (Settings->Winding != EPCGExWindingMutation::Unchanged)
			{
				ProjectionDetails = Settings->ProjectionDetails;
				if (!ProjectionDetails.Init(Context, VtxDataFacade)) { return; }

				PCGEx::InitArray(ProjectedPositions, VtxDataFacade->GetNum());
			}

			PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, ProjectionTaskGroup)

			ProjectionTaskGroup->OnCompleteCallback =
				[PCGEX_ASYNC_THIS_CAPTURE]()
				{
					PCGEX_ASYNC_THIS
					This->OnProjectionComplete();
				};

			ProjectionTaskGroup->OnSubLoopStartCallback =
				[PCGEX_ASYNC_THIS_CAPTURE](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
				{
					PCGEX_ASYNC_THIS
					if (This->BreakpointFilterManager)
					{
						TArray<int8>& Breaks = *This->Breakpoints;
						PCGEX_ASYNC_SUB_LOOP { Breaks[i] = This->BreakpointFilterManager->Test(i); }
					}

					if (This->ProjectedPositions)
					{
						This->ProjectionDetails.ProjectFlat(This->VtxDataFacade, *This->ProjectedPositions, StartIndex, Count);
					}
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
		ClusterProcessor->Breakpoints = Breakpoints;
		ClusterProcessor->ProjectedPositions = ProjectedPositions;
		return TBatch<FProcessor>::PrepareSingle(ClusterProcessor);
	}
}


#undef LOCTEXT_NAMESPACE


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
