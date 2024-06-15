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

void FPCGExPathfindingPlotEdgesContext::TryFindPath(const PCGExData::FPointIO* InPlotPoints) const
{
	PCGEX_SETTINGS_LOCAL(PathfindingPlotEdges)

	// TODO : Implement path-scoped extra weight management
	PCGExHeuristics::FLocalFeedbackHandler* LocalFeedbackHandler = HeuristicsHandler->MakeLocalFeedbackHandler(CurrentCluster);

	const PCGExCluster::FCluster* Cluster = CurrentCluster;
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

	PCGExData::FPointIO& PathPoints = OutputPaths->Emplace_GetRef(CurrentIO->GetIn(), PCGExData::EInit::NewOutput);
	PCGExGraph::CleanupClusterTags(&PathPoints, true);

	UPCGPointData* OutData = PathPoints.GetOut();

	PCGExGraph::CleanupVtxData(&PathPoints);

	TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();
	const TArray<FPCGPoint>& InPoints = CurrentIO->GetIn()->GetPoints();

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
		MutablePoints.Add(InPoints[Cluster->Nodes[VtxIndex].PointIndex]);
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

	PCGEX_DELETE(HeuristicsHandler)
	PCGEX_DELETE(Plots)
	PCGEX_DELETE(OutputPaths)
}


bool FPCGExPathfindingPlotEdgesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingPlotEdges)

	PCGEX_OPERATION_BIND(SearchAlgorithm, UPCGExSearchAStar)

	PCGEX_FWD(bAddSeedToPath)
	PCGEX_FWD(bAddGoalToPath)
	PCGEX_FWD(bAddPlotPointsToPath)

	Context->OutputPaths = new PCGExData::FPointIOCollection();
	Context->Plots = new PCGExData::FPointIOCollection();

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExPathfinding::SourcePlotsLabel);
	Context->Plots->Initialize(InContext, Sources, PCGExData::EInit::NoOutput);

	if (Context->Plots->IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing Plots Points."));
		return false;
	}

	Context->HeuristicsHandler = new PCGExHeuristics::THeuristicsHandler(Context);

	PCGEX_FWD(ProjectionSettings)

	return true;
}

bool FPCGExPathfindingPlotEdgesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathfindingPlotEdgesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingPlotEdges)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (!Context->ExecuteAutomation()) { return false; }

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges) { return false; }
			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	auto StartEdgeProcessing = [&]()
	{
		Context->SearchAlgorithm->PrepareForCluster(Context->CurrentCluster, Context->ClusterProjection);
		Context->HeuristicsHandler->PrepareForCluster(Context->CurrentCluster);
		Context->SetAsyncState(PCGExGraph::State_ProcessingEdges);
	};

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		if (!Context->AdvanceEdges(true))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		if (!Context->CurrentCluster) { return false; }

		if (Settings->bUseOctreeSearch)
		{
			if (Settings->SeedPicking.PickingMethod == EPCGExClusterClosestSearchMode::Node ||
				Settings->GoalPicking.PickingMethod == EPCGExClusterClosestSearchMode::Node)
			{
				Context->CurrentCluster->RebuildOctree(EPCGExClusterClosestSearchMode::Node);
			}

			if (Settings->SeedPicking.PickingMethod == EPCGExClusterClosestSearchMode::Edge ||
				Settings->GoalPicking.PickingMethod == EPCGExClusterClosestSearchMode::Edge)
			{
				Context->CurrentCluster->RebuildOctree(EPCGExClusterClosestSearchMode::Edge);
			}
		}

		if ((Context->bWaitingOnClusterProjection = Context->SearchAlgorithm->GetRequiresProjection()))
		{
			Context->SetState(PCGExCluster::State_ProjectingCluster);
			return false;
		}

		StartEdgeProcessing();
	}

	if (Context->IsState(PCGExCluster::State_ProjectingCluster)) { StartEdgeProcessing(); }

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		PCGEX_WAIT_ASYNC

		Context->HeuristicsHandler->CompleteClusterPreparation();

		if (Context->HeuristicsHandler->HasGlobalFeedback())
		{
			Context->CurrentPlotIndex = -1;
			Context->SetAsyncState(PCGExPathfinding::State_Pathfinding);
		}
		else
		{
			Context->Plots->ForEach(
				[&](PCGExData::FPointIO& PlotIO, const int32 Index)
				{
					if (PlotIO.GetNum() < 2)
					{
						PCGE_LOG(Warning, GraphAndLog, FTEXT("A plot has less than 2 points and will have no output."));
						return;
					}
					Context->GetAsyncManager()->Start<FPCGExPlotClusterPathTask>(Index, &PlotIO);
				});

			Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
		}
	}

	if (Context->IsState(PCGExPathfinding::State_Pathfinding))
	{
		// Advance to next plot
		Context->CurrentPlotIndex++;
		if (!Context->Plots->Pairs.IsValidIndex(Context->CurrentPlotIndex))
		{
			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
			return false;
		}

		PCGExData::FPointIO* PlotIO = Context->Plots->Pairs[Context->CurrentPlotIndex];
		if (PlotIO->GetNum() < 2) { return false; }

		Context->GetAsyncManager()->Start<FPCGExPlotClusterPathTask>(Context->CurrentPlotIndex, PlotIO);
		Context->SetAsyncState(PCGExPathfinding::State_WaitingPathfinding);
	}

	if (Context->IsState(PCGExPathfinding::State_WaitingPathfinding))
	{
		PCGEX_WAIT_ASYNC
		Context->SetState(PCGExPathfinding::State_Pathfinding);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC
		Context->SetState(PCGExGraph::State_ReadyForNextEdges);
	}

	if (Context->IsDone())
	{
		Context->OutputPaths->OutputTo(Context);
		Context->PostProcessOutputs();
	}

	return Context->IsDone();
}

bool FPCGExPlotClusterPathTask::ExecuteTask()
{
	const FPCGExPathfindingPlotEdgesContext* Context = Manager->GetContext<FPCGExPathfindingPlotEdgesContext>();
	PCGEX_SETTINGS(PathfindingPlotEdges)

	Context->TryFindPath(PointIO);

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
