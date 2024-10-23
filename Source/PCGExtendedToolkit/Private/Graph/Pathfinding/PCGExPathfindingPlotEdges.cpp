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
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPathfindingPlotEdgesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExGraph::OutputPathsLabel, "Paths output.", Required, {})
	return PinProperties;
}

void FPCGExPathfindingPlotEdgesContext::TryFindPath(
	const UPCGExSearchOperation* SearchOperation,
	const TSharedPtr<PCGExData::FPointIO>& InPlotPoints,
	const TSharedPtr<PCGExHeuristics::THeuristicsHandler>& HeuristicsHandler) const
{
	PCGEX_SETTINGS_LOCAL(PathfindingPlotEdges)

	const PCGExCluster::FCluster* Cluster = SearchOperation->Cluster;

	const TSharedPtr<PCGExHeuristics::FLocalFeedbackHandler> LocalFeedbackHandler = HeuristicsHandler->MakeLocalFeedbackHandler(Cluster);
	TArray<int32> Path;

	const int32 NumPlots = InPlotPoints->GetNum();

	for (int i = 1; i < NumPlots; i++)
	{
		FVector SeedPosition = InPlotPoints->GetInPoint(i - 1).Transform.GetLocation();
		FVector GoalPosition = InPlotPoints->GetInPoint(i).Transform.GetLocation();

		if (!SearchOperation->FindPath(
			SeedPosition, &Settings->SeedPicking,
			GoalPosition, &Settings->GoalPicking, HeuristicsHandler, Path, LocalFeedbackHandler))
		{
			// Failed
			if (Settings->bOmitCompletePathOnFailedPlot) { return; }
		}

		if (Settings->bAddPlotPointsToPath && i < NumPlots - 1) { Path.Add((i + 1) * -1); }
		SeedPosition = GoalPosition;
	}

	if (Settings->bClosedLoop)
	{
		const FVector SeedPosition = InPlotPoints->GetInPoint(InPlotPoints->GetNum() - 1).Transform.GetLocation();
		const FVector GoalPosition = InPlotPoints->GetInPoint(0).Transform.GetLocation();

		if (Settings->bAddPlotPointsToPath)
		{
			// Insert goal point as plot point
			Path.Add(NumPlots * -1);
		}

		if (!SearchOperation->FindPath(
			SeedPosition, &Settings->SeedPicking,
			GoalPosition, &Settings->GoalPicking, HeuristicsHandler, Path, LocalFeedbackHandler))
		{
			// Failed
			if (Settings->bOmitCompletePathOnFailedPlot) { return; }
		}
	}

	if (Path.Num() < 2 && !Settings->bAddSeedToPath && !Settings->bAddGoalToPath) { return; }

	const TSharedPtr<PCGExData::FPointIO> PathIO = OutputPaths->Emplace_GetRef<UPCGPointData>(Cluster->VtxIO.Pin()->GetIn(), PCGExData::EInit::NewOutput);
	PCGExGraph::CleanupClusterTags(PathIO, true);

	UPCGPointData* OutPathData = PathIO->GetOut();

	PCGExGraph::CleanupVtxData(PathIO);

	TArray<FPCGPoint>& MutablePoints = OutPathData->GetMutablePoints();
	const TArray<FPCGPoint>& InPoints = Cluster->VtxIO.Pin()->GetIn()->GetPoints();

	MutablePoints.Reserve(Path.Num() + 2);

	if (Settings->bAddSeedToPath) { MutablePoints.Add_GetRef(InPlotPoints->GetInPoint(0)).MetadataEntry = PCGInvalidEntryKey; }

	const TArray<int32>& VtxPointIndices = SearchOperation->Cluster->GetVtxPointIndices();
	int32 LastIndex = -1;
	for (const int32 VtxIndex : Path)
	{
		if (VtxIndex < 0) // Plot point
		{
			MutablePoints.Add_GetRef(InPlotPoints->GetInPoint((VtxIndex * -1) - 1)).MetadataEntry = PCGInvalidEntryKey;
			continue;
		}

		if (LastIndex == VtxIndex) { continue; } //Skip duplicates
		MutablePoints.Add(InPoints[VtxPointIndices[VtxIndex]]);
		LastIndex = VtxIndex;
	}

	if (Settings->bAddGoalToPath && !Settings->bClosedLoop)
	{
		MutablePoints.Add_GetRef(InPlotPoints->GetInPoint(InPlotPoints->GetNum() - 1)).MetadataEntry = PCGInvalidEntryKey;
	}

	PathIO->Tags->Append(InPlotPoints->Tags.ToSharedRef());

	if (!Settings->bClosedLoop)
	{
		if (Settings->bTagIfOpenPath) { PathIO->Tags->Add(Settings->IsOpenPathTag); }
	}
	else
	{
		if (Settings->bTagIfClosedLoop) { PathIO->Tags->Add(Settings->IsClosedLoopTag); }
	}
}

PCGEX_INITIALIZE_ELEMENT(PathfindingPlotEdges)

bool FPCGExPathfindingPlotEdgesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingPlotEdges)

	PCGEX_OPERATION_BIND(SearchAlgorithm, UPCGExSearchOperation)

	Context->OutputPaths = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->Plots = MakeShared<PCGExData::FPointIOCollection>(Context);

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExGraph::SourcePlotsLabel);
	Context->Plots->Initialize(Sources, PCGExData::EInit::NoOutput);

	for (int i = 0; i < Context->Plots->Num(); i++)
	{
		const PCGExData::FPointIO* Plot = Context->Plots->Pairs[i].Get();
		if (Plot->GetNum() < 2)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Pruned plot with < 2 points."));
			Context->Plots->Pairs.RemoveAt(i);
			i--;
		}
	}

	if (Context->Plots->IsEmpty())
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
	bool FPCGExPlotClusterPathTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		const FPCGExPathfindingPlotEdgesContext* Context = AsyncManager->GetContext<FPCGExPathfindingPlotEdgesContext>();
		PCGEX_SETTINGS(PathfindingPlotEdges)

		Context->TryFindPath(SearchOperation, Plots->Pairs[TaskIndex], Heuristics);

		if (bInlined && Plots->Pairs.IsValidIndex(TaskIndex + 1))
		{
			InternalStart<FPCGExPlotClusterPathTask>(TaskIndex + 1, PointIO, SearchOperation, Plots, Heuristics, true);
		}

		return true;
	}

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

		if (IsTrivial())
		{
			// Naturally accounts for global heuristics
			for (const TSharedPtr<PCGExData::FPointIO> PlotIO : Context->Plots->Pairs)
			{
				Context->TryFindPath(SearchOperation, PlotIO, HeuristicsHandler);
			}

			return true;
		}

		if (HeuristicsHandler->HasGlobalFeedback())
		{
			AsyncManager->Start<FPCGExPlotClusterPathTask>(0, VtxDataFacade->Source, SearchOperation, Context->Plots, HeuristicsHandler, true);
		}
		else
		{
			for (int i = 0; i < Context->Plots->Num(); i++)
			{
				AsyncManager->Start<FPCGExPlotClusterPathTask>(i, VtxDataFacade->Source, SearchOperation, Context->Plots, HeuristicsHandler, false);
			}
		}

		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
