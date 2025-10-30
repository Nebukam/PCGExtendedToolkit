// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingPlotEdges.h"


#include "PCGExPointsProcessor.h"
#include "PCGParamData.h"
#include "Graph/PCGExGraph.h"
#include "Algo/Reverse.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTag.h"
#include "Data/PCGExPointIO.h"


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDistance.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"
#include "Graph/Pathfinding/Search/PCGExSearchAStar.h"
#include "Paths/PCGExPaths.h"

#define LOCTEXT_NAMESPACE "PCGExPathfindingPlotEdgesElement"
#define PCGEX_NAMESPACE PathfindingPlotEdges

#if WITH_EDITOR
void UPCGExPathfindingPlotEdgesSettings::PostInitProperties()
{
	if (!HasAnyFlags(RF_ClassDefaultObject) && IsInGameThread())
	{
		if (!SearchAlgorithm) { SearchAlgorithm = NewObject<UPCGExSearchAStar>(this, TEXT("SearchAlgorithm")); }
	}
	Super::PostInitProperties();
}

void UPCGExPathfindingPlotEdgesSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

TArray<FPCGPinProperties> UPCGExPathfindingPlotEdgesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::SourcePlotsLabel, "Plot points for pathfinding.", Required)
	PCGEX_PIN_FACTORIES(PCGExGraph::SourceHeuristicsLabel, "Heuristics.", Required, FPCGExDataTypeInfoHeuristics::AsId())
	PCGEX_PIN_OPERATION_OVERRIDES(PCGExPathfinding::SourceOverridesSearch)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPathfindingPlotEdgesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExPaths::OutputPathsLabel, "Paths output.", Required)
	return PinProperties;
}

void FPCGExPathfindingPlotEdgesContext::BuildPath(const TSharedPtr<PCGExPathfinding::FPlotQuery>& Query, const TSharedPtr<PCGExData::FPointIO>& PathIO) const
{
	PCGEX_SETTINGS_LOCAL(PathfindingPlotEdges)

	if (!PathIO) { return; }
	
	bool bAddGoal = Settings->bAddGoalToPath ? (!Query->bIsClosedLoop || !Settings->bAddSeedToPath) : false;

	int32 NumPoints = Query->SubQueries.Num() + 2;
	int32 ValidPlotIndex = 0;

	int32 MaxQueryNumPoints = 0;
	for (const TSharedPtr<PCGExPathfinding::FPathQuery>& PathQuery : Query->SubQueries)
	{
		if (!PathQuery->IsQuerySuccessful()) { continue; }
		MaxQueryNumPoints = FMath::Max(MaxQueryNumPoints, PathQuery->PathNodes.Num());
		NumPoints += PathQuery->PathNodes.Num();
		ValidPlotIndex++;
	}

	if (ValidPlotIndex == 0) { return; } // No path could be resolved

	//

	TArray<int32> IndicesBuffer;
	IndicesBuffer.Reserve(MaxQueryNumPoints);

	// Create easy-to-track scopes for indices
	PCGEx::FReadWriteScope PlotScope(ValidPlotIndex + 2, false);
	PCGEx::FReadWriteScope ClusterScope(NumPoints, false);

	int32 WriteIndex = 0;

	if (Settings->bAddSeedToPath) { PlotScope.Add(Query->SubQueries[0]->Seed.Point.Index, WriteIndex++); }

	for (int i = 0; i < Query->SubQueries.Num(); i++)
	{
		TSharedPtr<PCGExPathfinding::FPathQuery> PathQuery = Query->SubQueries[i];
		if (Settings->bAddPlotPointsToPath && i != 0) { PlotScope.Add(PathQuery->Seed.Point.Index, WriteIndex++); }

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
			PathQuery->AppendNodePoints(IndicesBuffer, TruncateStart, TruncateEnd);
		}
		else if (Settings->PathComposition == EPCGExPathComposition::Edges)
		{
			PathQuery->AppendEdgePoints(IndicesBuffer);
		}
		else if (Settings->PathComposition == EPCGExPathComposition::VtxAndEdges)
		{
			// TODO : Implement
		}

		ClusterScope.Add(IndicesBuffer, WriteIndex);
		IndicesBuffer.Reset();
	}

	if (bAddGoal) { PlotScope.Add(Query->SubQueries.Last()->Goal.Point.Index, WriteIndex++); }

	if (Settings->PathComposition == EPCGExPathComposition::Vtx)
	{
		if (ClusterScope.Num() < 2) { return; }
	}
	else if (Settings->PathComposition == EPCGExPathComposition::Edges)
	{
		if (ClusterScope.Num() < 1) { return; }
	}
	else if (Settings->PathComposition == EPCGExPathComposition::VtxAndEdges)
	{
		// TODO : Implement
		return;
	}

	if (!Settings->PathOutputDetails.Validate(WriteIndex)) { return; }

	PathIO->Enable();
	PathIO->IOIndex = Query->QueryIndex;

	PCGEX_MAKE_SHARED(PathDataFacade, PCGExData::FFacade, PathIO.ToSharedRef())
	PCGEx::SetNumPointsAllocated(PathIO->GetOut(), ClusterScope.Num() + PlotScope.Num(), PathIO->GetAllocations());

	// Commit read/write scopes
	PlotScope.CopyPoints(Query->PlotFacade->GetIn(), PathIO->GetOut(), true, true);
	ClusterScope.CopyProperties(PathIO->GetIn(), PathIO->GetOut(), EPCGPointNativeProperties::All);

	PCGExGraph::CleanupClusterData(PathIO);

	PathIO->Tags->Append(Query->PlotFacade->Source->Tags.ToSharedRef());

	PCGExPaths::SetClosedLoop(PathIO->GetOut(), Settings->bClosedLoop);
}

PCGEX_INITIALIZE_ELEMENT(PathfindingPlotEdges)

PCGEX_ELEMENT_BATCH_EDGE_IMPL(PathfindingPlotEdges)

bool FPCGExPathfindingPlotEdgesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingPlotEdges)

	PCGEX_OPERATION_BIND(SearchAlgorithm, UPCGExSearchInstancedFactory, PCGExPathfinding::SourceOverridesSearch)

	Context->OutputPaths = MakeShared<PCGExData::FPointIOCollection>(Context);
	PCGEX_MAKE_SHARED(Plots, PCGExData::FPointIOCollection, Context)

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExGraph::SourcePlotsLabel);
	Plots->Initialize(Sources, PCGExData::EIOInit::NoInit);

	Context->Plots.Reserve(Plots->Num());
	for (const TSharedPtr<PCGExData::FPointIO>& PlotIO : Plots->Pairs)
	{
		if (PlotIO->GetNum() < 2)
		{
			if (!Settings->bQuietInvalidPlotWarning) { PCGE_LOG(Warning, GraphAndLog, FTEXT("Pruned plot with < 2 points.")); }
			continue;
		}

		TSharedPtr<PCGExData::FFacade> PlotFacade = MakeShared<PCGExData::FFacade>(PlotIO.ToSharedRef());
		Context->Plots.Add(PlotFacade);
	}

	if (Context && Context->Plots.IsEmpty())
	{
		PCGEX_LOG_MISSING_INPUT(Context, FTEXT("Missing valid Plots."))
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
		if (!Context->StartProcessingClusters(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
			{
				NewBatch->SetWantsHeuristics(true);
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->OutputPaths->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExPathfindingPlotEdges
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathfindingPlotEdges::Process);

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		if (Settings->bUseOctreeSearch)
		{
			if (Settings->SeedPicking.PickingMethod == EPCGExClusterClosestSearchMode::Vtx ||
				Settings->GoalPicking.PickingMethod == EPCGExClusterClosestSearchMode::Vtx)
			{
				Cluster->RebuildOctree(EPCGExClusterClosestSearchMode::Vtx);
			}

			if (Settings->SeedPicking.PickingMethod == EPCGExClusterClosestSearchMode::Edge ||
				Settings->GoalPicking.PickingMethod == EPCGExClusterClosestSearchMode::Edge)
			{
				Cluster->RebuildOctree(EPCGExClusterClosestSearchMode::Edge);
			}
		}

		TSharedPtr<PCGExData::FPointIO> ReferenceIO = nullptr;

		if (Settings->PathComposition == EPCGExPathComposition::Vtx) { ReferenceIO = VtxDataFacade->Source; }
		else if (Settings->PathComposition == EPCGExPathComposition::Edges) { ReferenceIO = EdgeDataFacade->Source; }
		else if (Settings->PathComposition == EPCGExPathComposition::VtxAndEdges)
		{
			// TODO : Implement
		}
		
		SearchOperation = Context->SearchAlgorithm->CreateOperation(); // Create a local copy
		SearchOperation->PrepareForCluster(Cluster.Get());
;
		const int32 NumPlots = Context->Plots.Num();
		PCGEx::InitArray(Queries, NumPlots);
		QueriesIO.Init(nullptr, NumPlots);
		
		Context->OutputPaths->IncreaseReserve(NumPlots);
		for (int i = 0; i < NumPlots; i++)
		{
			PCGEX_MAKE_SHARED(Query, PCGExPathfinding::FPlotQuery, Cluster.ToSharedRef(), Settings->bClosedLoop, i)
			Queries[i] = Query;
			QueriesIO[i] = Context->OutputPaths->Emplace_GetRef<UPCGPointArrayData>(ReferenceIO, PCGExData::EIOInit::New);
			QueriesIO[i]->Disable();
		}

		bForceSingleThreadedProcessRange = HeuristicsHandler->HasGlobalFeedback() || !Settings->bGreedyQueries;
		if (bForceSingleThreadedProcessRange) { SearchAllocations = SearchOperation->NewAllocations(); }

		StartParallelLoopForRange(Queries.Num(), 1);
		return true;
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		PCGEX_SCOPE_LOOP(Index)
		{
			TSharedPtr<PCGExPathfinding::FPlotQuery> Query = Queries[Index];
			Query->BuildPlotQuery(Context->Plots[Index], Settings->SeedPicking, Settings->GoalPicking);
			Query->OnCompleteCallback = [PCGEX_ASYNC_THIS_CAPTURE](const TSharedPtr<PCGExPathfinding::FPlotQuery>& Plot)
			{
				PCGEX_ASYNC_THIS
				This->Context->BuildPath(Plot, This->QueriesIO[Plot->QueryIndex]);
				Plot->Cleanup();
			};
			Query->FindPaths(AsyncManager, SearchOperation, SearchAllocations, HeuristicsHandler);
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
