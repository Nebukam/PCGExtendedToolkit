// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingPlotEdges.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Algo/Reverse.h"


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDistance.h"
#include "Graph/Pathfinding/Search/PCGExSearchAStar.h"

#define LOCTEXT_NAMESPACE "PCGExPathfindingPlotEdgesElement"
#define PCGEX_NAMESPACE PathfindingPlotEdges

#if WITH_EDITOR
void UPCGExPathfindingPlotEdgesSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

TArray<FPCGPinProperties> UPCGExPathfindingPlotEdgesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::SourcePlotsLabel, "Plot points for pathfinding.", Required, {})
	PCGEX_PIN_PARAMS(PCGExGraph::SourceHeuristicsLabel, "Heuristics.", Normal, {})
	PCGEX_PIN_OPERATION_OVERRIDES(PCGExPathfinding::SourceOverridesSearch)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPathfindingPlotEdgesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExGraph::OutputPathsLabel, "Paths output.", Required, {})
	return PinProperties;
}

void FPCGExPathfindingPlotEdgesContext::BuildPath(const TSharedPtr<PCGExPathfinding::FPlotQuery>& Query) const
{
	PCGEX_SETTINGS_LOCAL(PathfindingPlotEdges)

	bool bAddGoal = Settings->bAddGoalToPath ? (!Query->bIsClosedLoop || !Settings->bAddSeedToPath) : false;

	int32 NumPoints = Query->SubQueries.Num() + 2;
	int32 ValidPlotIndex = 0;

	for (const TSharedPtr<PCGExPathfinding::FPathQuery>& PathQuery : Query->SubQueries)
	{
		if (!PathQuery->IsQuerySuccessful()) { continue; }
		NumPoints += PathQuery->PathNodes.Num();
		ValidPlotIndex++;
	}

	if (ValidPlotIndex == 0) { return; } // No path could be resolved

	//

	TArray<FPCGPoint> MutablePoints;
	MutablePoints.Reserve(NumPoints);

	auto AddPlotPoint = [&](int32 Index)
	{
		MutablePoints.Add_GetRef(Query->PlotFacade->Source->GetInPoint(Index)).MetadataEntry = PCGInvalidEntryKey;
	};

	if (Settings->bAddSeedToPath) { AddPlotPoint(Query->SubQueries[0]->Seed.SourceIndex); }

	for (int i = 0; i < Query->SubQueries.Num(); i++)
	{
		TSharedPtr<PCGExPathfinding::FPathQuery> PathQuery = Query->SubQueries[i];
		if (Settings->bAddPlotPointsToPath && i != 0) { AddPlotPoint(PathQuery->Seed.SourceIndex); }

		if (!PathQuery->IsQuerySuccessful()) { continue; }

		int32 TruncateStart = 0;
		int32 TruncateEnd = 0;

		// First path, full
		if (Settings->bAddPlotPointsToPath || i == 0) { TruncateStart = TruncateEnd = 0; }
		// Last path, if closed loop, truncated both start & end
		else if (Settings->bClosedLoop && i == Query->SubQueries.Num() - 1) { TruncateStart = TruncateEnd = 1; }
		// Body path, truncated start
		else { TruncateStart = 1; }

		if (Settings->PathComposition == EPCGExPathComposition::Vtx)
		{
			PathQuery->AppendNodePoints(MutablePoints, TruncateStart, TruncateEnd);
		}
		else if (Settings->PathComposition == EPCGExPathComposition::Edges)
		{
			PathQuery->AppendEdgePoints(MutablePoints);
		}
		else if (Settings->PathComposition == EPCGExPathComposition::VtxAndEdges)
		{
			// TODO : Implement
		}
	}

	if (bAddGoal) { AddPlotPoint(Query->SubQueries.Last()->Goal.SourceIndex); }

	TSharedPtr<PCGExData::FPointIO> ReferenceIO = nullptr;

	if (Settings->PathComposition == EPCGExPathComposition::Vtx)
	{
		if (MutablePoints.Num() < 2) { return; }
		ReferenceIO = Query->Cluster->VtxIO.Pin();
	}
	else if (Settings->PathComposition == EPCGExPathComposition::Edges)
	{
		if (MutablePoints.Num() < 1) { return; }
		ReferenceIO = Query->Cluster->EdgesIO.Pin();
	}
	else if (Settings->PathComposition == EPCGExPathComposition::VtxAndEdges)
	{
		// TODO : Implement
	}

	const TSharedPtr<PCGExData::FPointIO> PathIO = OutputPaths->Emplace_GetRef<UPCGPointData>(ReferenceIO->GetIn(), PCGExData::EIOInit::NewOutput);
	const TSharedPtr<PCGExData::FFacade> PathDataFacade = MakeShared<PCGExData::FFacade>(PathIO.ToSharedRef());
	PathDataFacade->GetMutablePoints() = MutablePoints;

	PCGExGraph::CleanupClusterTags(PathIO, true);
	PCGExGraph::CleanupVtxData(PathIO);

	PathIO->Tags->Append(Query->PlotFacade->Source->Tags.ToSharedRef());

	if (!Settings->bClosedLoop) { if (Settings->bTagIfOpenPath) { PathIO->Tags->Add(Settings->IsOpenPathTag); } }
	else { if (Settings->bTagIfClosedLoop) { PathIO->Tags->Add(Settings->IsClosedLoopTag); } }
}

PCGEX_INITIALIZE_ELEMENT(PathfindingPlotEdges)

bool FPCGExPathfindingPlotEdgesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingPlotEdges)

	PCGEX_OPERATION_BIND(SearchAlgorithm, UPCGExSearchOperation, PCGExPathfinding::SourceOverridesSearch)

	Context->OutputPaths = MakeShared<PCGExData::FPointIOCollection>(Context);
	TSharedPtr<PCGExData::FPointIOCollection> Plots = MakeShared<PCGExData::FPointIOCollection>(Context);

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExGraph::SourcePlotsLabel);
	Plots->Initialize(Sources, PCGExData::EIOInit::NoOutput);

	Context->Plots.Reserve(Plots->Num());
	for (const TSharedPtr<PCGExData::FPointIO>& PlotIO : Plots->Pairs)
	{
		if (PlotIO->GetNum() < 2) { PCGE_LOG(Warning, GraphAndLog, FTEXT("Pruned plot with < 2 points.")); }
		TSharedPtr<PCGExData::FFacade> PlotFacade = MakeShared<PCGExData::FFacade>(PlotIO.ToSharedRef());
		Context->Plots.Add(PlotFacade);
	}

	if (Context->Plots.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing valid Plots."));
		return false;
	}


	return true;
}

bool FPCGExPathfindingPlotEdgesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathfindingPlotEdgesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingPlotEdges)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatchWithHeuristics<PCGExPathfindingPlotEdge::FProcessor>>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::TBatchWithHeuristics<PCGExPathfindingPlotEdge::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)

	Context->OutputPaths->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExPathfindingPlotEdge
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathfindingPlotEdge::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		if (Settings->bUseOctreeSearch)
		{
			if (Settings->SeedPicking.PickingMethod == EPCGExClusterClosestSearchMode::Node ||
				Settings->GoalPicking.PickingMethod == EPCGExClusterClosestSearchMode::Node)
			{
				Cluster->RebuildOctree(EPCGExClusterClosestSearchMode::Node);
			}

			if (Settings->SeedPicking.PickingMethod == EPCGExClusterClosestSearchMode::Edge ||
				Settings->GoalPicking.PickingMethod == EPCGExClusterClosestSearchMode::Edge)
			{
				Cluster->RebuildOctree(EPCGExClusterClosestSearchMode::Edge);
			}
		}

		SearchOperation = Context->SearchAlgorithm->CopyOperation<UPCGExSearchOperation>(); // Create a local copy
		SearchOperation->PrepareForCluster(Cluster.Get());

		PCGEx::InitArray(Queries, Context->Plots.Num());
		for (int i = 0; i < Queries.Num(); i++)
		{
			const TSharedPtr<PCGExPathfinding::FPlotQuery> Query = MakeShared<PCGExPathfinding::FPlotQuery>(Cluster.ToSharedRef(), Settings->bClosedLoop);
			Queries[i] = Query;
		}

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, ResolveQueriesTask)
		TWeakPtr<FProcessor> WeakPtr = SharedThis(this);
		ResolveQueriesTask->OnIterationCallback = [WeakPtr](const int32 Index, const int32 Count, const int32 LoopIdx)
		{
			TSharedPtr<FProcessor> This = WeakPtr.Pin();
			if (!This) { return; }

			TSharedPtr<PCGExPathfinding::FPlotQuery> Query = This->Queries[Index];
			Query->BuildPlotQuery(This->Context->Plots[Index], This->Settings->SeedPicking, This->Settings->GoalPicking);
			Query->FindPaths(This->AsyncManager, This->SearchOperation, This->HeuristicsHandler);
			Query->OnCompleteCallback = [WeakPtr](const TSharedPtr<PCGExPathfinding::FPlotQuery>& Plot)
			{
				TSharedPtr<FProcessor> NestedThis = WeakPtr.Pin();
				if (!NestedThis) { return; }
				NestedThis->Context->BuildPath(Plot);
				Plot->Cleanup();
			};
		};

		ResolveQueriesTask->StartIterations(Queries.Num(), 1, HeuristicsHandler->HasGlobalFeedback(), false);
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
