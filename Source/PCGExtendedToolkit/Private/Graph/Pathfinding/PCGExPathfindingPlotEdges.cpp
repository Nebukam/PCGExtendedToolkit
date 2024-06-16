// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingPlotEdges.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "PCGExPathfinding.cpp"
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
	PCGEX_PIN_POINTS(PCGExPathfinding::SourcePlotsLabel, "Plot points for pathfinding.", Required, {})
	PCGEX_PIN_PARAMS(PCGExPathfinding::SourceHeuristicsLabel, "Heuristics.", Normal, {})
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
	const PCGExData::FPointIO* InPlotPoints, PCGExHeuristics::THeuristicsHandler* HeuristicsHandler) const
{
	PCGEX_SETTINGS_LOCAL(PathfindingPlotEdges)

	const PCGExCluster::FCluster* Cluster = SearchOperation->Cluster;

	// TODO : Implement path-scoped extra weight management
	PCGExHeuristics::FLocalFeedbackHandler* LocalFeedbackHandler = HeuristicsHandler->MakeLocalFeedbackHandler(Cluster);
	TArray<int32> Path;

	const int32 NumPlots = InPlotPoints->GetNum();

	for (int i = 1; i < NumPlots; i++)
	{
		FVector SeedPosition = InPlotPoints->GetInPoint(i - 1).Transform.GetLocation();
		FVector GoalPosition = InPlotPoints->GetInPoint(i).Transform.GetLocation();

		if (!SearchAlgorithm->FindPath(
			SeedPosition, &Settings->SeedPicking,
			GoalPosition, &Settings->GoalPicking, HeuristicsHandler, Path, LocalFeedbackHandler))
		{
			// Failed
		}

		if (bAddPlotPointsToPath && i < NumPlots - 1) { Path.Add((i + 1) * -1); }

		SeedPosition = GoalPosition;
	}

	PCGExData::FPointIO& PathPoints = OutputPaths->Emplace_GetRef(Cluster->PointsIO->GetIn(), PCGExData::EInit::NewOutput);
	PCGExGraph::CleanupClusterTags(&PathPoints, true);

	UPCGPointData* OutData = PathPoints.GetOut();

	PCGExGraph::CleanupVtxData(&PathPoints);

	TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();
	const TArray<FPCGPoint>& InPoints = Cluster->PointsIO->GetIn()->GetPoints();

	MutablePoints.Reserve(Path.Num() + 2);

	if (bAddSeedToPath) { MutablePoints.Add_GetRef(InPlotPoints->GetInPoint(0)).MetadataEntry = PCGInvalidEntryKey; }
	int32 LastIndex = -1;
	for (const int32 VtxIndex : Path)
	{
		if (VtxIndex < 0) // Plot point
		{
			MutablePoints.Add_GetRef(InPlotPoints->GetInPoint((VtxIndex * -1) - 1)).MetadataEntry = PCGInvalidEntryKey;
			continue;
		}

		if (LastIndex == VtxIndex) { continue; } //Skip duplicates
		MutablePoints.Add(InPoints[SearchOperation->Cluster->Nodes[VtxIndex].PointIndex]);
		LastIndex = VtxIndex;
	}
	if (bAddGoalToPath) { MutablePoints.Add_GetRef(InPlotPoints->GetInPoint(InPlotPoints->GetNum() - 1)).MetadataEntry = PCGInvalidEntryKey; }

	PathPoints.Tags->Append(InPlotPoints->Tags);

	PCGEX_DELETE(LocalFeedbackHandler)
}

PCGEX_INITIALIZE_ELEMENT(PathfindingPlotEdges)

void UPCGExPathfindingPlotEdgesSettings::PostInitProperties()
{
	Super::PostInitProperties();

	PCGEX_OPERATION_DEFAULT(SearchAlgorithm, UPCGExSearchAStar)
	PCGEX_OPERATION_DEFAULT(Heuristics, UPCGExHeuristicDistance)
}

FPCGExPathfindingPlotEdgesContext::~FPCGExPathfindingPlotEdgesContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(Plots)
	PCGEX_DELETE(OutputPaths)
}


bool FPCGExPathfindingPlotEdgesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingPlotEdges)

	PCGEX_OPERATION_BIND(SearchAlgorithm, UPCGExSearchAStar)

	Context->OutputPaths = new PCGExData::FPointIOCollection();
	Context->Plots = new PCGExData::FPointIOCollection();

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExPathfinding::SourcePlotsLabel);
	Context->Plots->Initialize(InContext, Sources, PCGExData::EInit::NoOutput);

	for (int i = 0; i < Context->Plots->Num(); i++)
	{
		const PCGExData::FPointIO* Plot = Context->Plots->Pairs[i];
		if (Plot->GetNum() < 2)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Pruned plot with < 2 points."));
			Context->Plots->Pairs.RemoveAt(i);
			i--;
			PCGEX_DELETE(Plot)
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

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatch<PCGExPathfindingPlotEdge::FProcessor>>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExClusterMT::TBatch<PCGExPathfindingPlotEdge::FProcessor>* NewBatch) { return; },
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	if (Context->IsDone())
	{
		Context->OutputPaths->OutputTo(Context);
		Context->PostProcessOutputs();
	}

	return Context->IsDone();
}

namespace PCGExPathfindingPlotEdge
{
	bool FPCGExPlotClusterPathTask::ExecuteTask()
	{
		const FPCGExPathfindingPlotEdgesContext* Context = Manager->GetContext<FPCGExPathfindingPlotEdgesContext>();
		PCGEX_SETTINGS(PathfindingPlotEdges)

		Context->TryFindPath(SearchOperation, Plots->Pairs[TaskIndex], Heuristics);

		if (bInlined && Plots->Pairs.IsValidIndex(TaskIndex + 1))
		{
			Manager->Start<FPCGExPlotClusterPathTask>(TaskIndex + 1, PointIO, SearchOperation, Plots, Heuristics, true);
		}

		return true;
	}

	FProcessor::FProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges):
		FClusterProcessor(InVtx, InEdges)
	{
		bRequiresHeuristics = true;
	}

	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(FPCGExAsyncManager* AsyncManager)
	{
		PCGEX_SETTINGS(PathfindingPlotEdges)
		const FPCGExPathfindingPlotEdgesContext* TypedContext = static_cast<FPCGExPathfindingPlotEdgesContext*>(Context);

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

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

		SearchOperation = TypedContext->SearchAlgorithm->CopyOperation<UPCGExSearchOperation>(); // Create a local copy
		SearchOperation->PrepareForCluster(Cluster);

		if (IsTrivial())
		{
			// Naturally accounts for global heuristics
			for (const PCGExData::FPointIO* PlotIO : TypedContext->Plots->Pairs)
			{
				TypedContext->TryFindPath(SearchOperation, PlotIO, HeuristicsHandler);
			}

			return true;
		}

		if (HeuristicsHandler->HasGlobalFeedback())
		{
			AsyncManagerPtr->Start<FPCGExPlotClusterPathTask>(0, VtxIO, SearchOperation, TypedContext->Plots, HeuristicsHandler, true);
		}
		else
		{
			for (int i = 0; i < TypedContext->Plots->Num(); i++)
			{
				AsyncManagerPtr->Start<FPCGExPlotClusterPathTask>(i, VtxIO, SearchOperation, TypedContext->Plots, HeuristicsHandler, false);
			}
		}

		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
