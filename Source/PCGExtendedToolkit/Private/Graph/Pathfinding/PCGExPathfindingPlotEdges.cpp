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
	if (Heuristics) { Heuristics->UpdateUserFacingInfos(); }
	HeuristicsModifiers.UpdateUserFacingInfos();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

TArray<FPCGPinProperties> UPCGExPathfindingPlotEdgesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	FPCGPinProperties& PinPropertySeeds = PinProperties.Emplace_GetRef(PCGExPathfinding::SourcePlotsLabel, EPCGDataType::Point, true, true);

#if WITH_EDITOR
	PinPropertySeeds.Tooltip = FTEXT("Plot points for pathfinding.");
#endif

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPathfindingPlotEdgesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPathsOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputPathsLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPathsOutput.Tooltip = FTEXT("Paths output.");
#endif

	return PinProperties;
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

	if (HeuristicsModifiers) { HeuristicsModifiers->Cleanup(); }

	PCGEX_DELETE(GlobalExtraWeights)
	PCGEX_DELETE(Plots)
	PCGEX_DELETE(OutputPaths)
}


bool FPCGExPathfindingPlotEdgesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingPlotEdges)

	PCGEX_OPERATION_BIND(SearchAlgorithm, UPCGExSearchAStar)
	PCGEX_OPERATION_BIND(Heuristics, UPCGExHeuristicDistance)

	PCGEX_FWD(bAddSeedToPath)
	PCGEX_FWD(bAddGoalToPath)
	PCGEX_FWD(bAddPlotPointsToPath)

	PCGEX_FWD(bWeightUpVisited)
	PCGEX_FWD(VisitedPointsWeightFactor)
	PCGEX_FWD(VisitedEdgesWeightFactor)

	Context->OutputPaths = new PCGExData::FPointIOCollection();
	Context->Plots = new PCGExData::FPointIOCollection();

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExPathfinding::SourcePlotsLabel);
	Context->Plots->Initialize(InContext, Sources, PCGExData::EInit::NoOutput);

	if (Context->Plots->IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing Plots Points."));
		return false;
	}

	Context->SearchAlgorithm->SearchMode = Settings->NodePickingMode;

	Context->HeuristicsModifiers = const_cast<FPCGExHeuristicModifiersSettings*>(&Settings->HeuristicsModifiers);
	Context->HeuristicsModifiers->LoadCurves();
	Context->Heuristics->ReferenceWeight = Context->HeuristicsModifiers->ReferenceWeight;

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

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points have no associated edges."));
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
			}
			else
			{
				Context->SetState(PCGExGraph::State_ReadyForNextEdges);
			}
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		if (!Context->AdvanceEdges(true))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		if (!Context->CurrentCluster)
		{
			PCGEX_INVALID_CLUSTER_LOG
			return false;
		}

		if (Settings->bUseOctreeSearch) { Context->CurrentCluster->RebuildOctree(Settings->NodePickingMode); }

		Context->SetState(PCGExCluster::State_ProjectingCluster);
	}

	if (Context->IsState(PCGExCluster::State_ProjectingCluster))
	{
		if (Context->SearchAlgorithm->GetRequiresProjection())
		{
			if (!Context->ProjectCluster()) { return false; }
		}

		Context->SearchAlgorithm->PrepareForCluster(Context->CurrentCluster, Context->ClusterProjection);
		Context->GetAsyncManager()->Start<FPCGExCompileModifiersTask>(0, Context->CurrentIO, Context->CurrentEdges, Context->HeuristicsModifiers);
		Context->SetAsyncState(PCGExGraph::State_ProcessingEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		PCGEX_WAIT_ASYNC

		PCGEX_DELETE(Context->GlobalExtraWeights);
		Context->Heuristics->PrepareForData(Context->CurrentCluster);

		if (Settings->bWeightUpVisited && Settings->bGlobalVisitedWeight)
		{
			Context->GlobalExtraWeights = new PCGExPathfinding::FExtraWeights(
				Context->CurrentCluster,
				Context->VisitedPointsWeightFactor,
				Context->VisitedEdgesWeightFactor);

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

		Context->GetAsyncManager()->Start<FPCGExPlotClusterPathTask>(Context->CurrentPlotIndex, PlotIO, Context->GlobalExtraWeights);
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

	if (Context->IsDone()) { Context->OutputPaths->OutputTo(Context); }

	return Context->IsDone();
}

bool FPCGExPlotClusterPathTask::ExecuteTask()
{
	const FPCGExPathfindingPlotEdgesContext* Context = Manager->GetContext<FPCGExPathfindingPlotEdgesContext>();

	const PCGExCluster::FCluster* Cluster = Context->CurrentCluster;
	TArray<int32> Path;

	PCGExPathfinding::FExtraWeights* ExtraWeights = GlobalExtraWeights;
	if (Context->bWeightUpVisited && !ExtraWeights)
	{
		ExtraWeights = new PCGExPathfinding::FExtraWeights(
			Cluster,
			Context->VisitedPointsWeightFactor,
			Context->VisitedEdgesWeightFactor);
	}

	const int32 NumPlots = PointIO->GetNum();

	for (int i = 1; i < NumPlots; i++)
	{
		FVector SeedPosition = PointIO->GetInPoint(i - 1).Transform.GetLocation();
		FVector GoalPosition = PointIO->GetInPoint(i).Transform.GetLocation();

		if (!Context->SearchAlgorithm->FindPath(
			SeedPosition, GoalPosition,
			Context->Heuristics, Context->HeuristicsModifiers, Path, ExtraWeights))
		{
			// Failed
		}

		if (Context->bAddPlotPointsToPath && i < NumPlots - 1) { Path.Add((i + 1) * -1); }

		SeedPosition = GoalPosition;
	}

	PCGExData::FPointIO& PathPoints = Context->OutputPaths->Emplace_GetRef(Context->GetCurrentIn(), PCGExData::EInit::NewOutput);
	UPCGPointData* OutData = PathPoints.GetOut();

	PCGExGraph::CleanupVtxData(&PathPoints);

	TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();
	const TArray<FPCGPoint>& InPoints = Context->GetCurrentIn()->GetPoints();

	MutablePoints.Reserve(Path.Num() + 2);

	if (Context->bAddSeedToPath) { MutablePoints.Add_GetRef(PointIO->GetInPoint(0)).MetadataEntry = PCGInvalidEntryKey; }
	int32 LastIndex = -1;
	for (const int32 VtxIndex : Path)
	{
		if (VtxIndex < 0) // Plot point
		{
			MutablePoints.Add_GetRef(PointIO->GetInPoint((VtxIndex * -1) - 1)).MetadataEntry = PCGInvalidEntryKey;
			continue;
		}

		if (LastIndex == VtxIndex) { continue; } //Skip duplicates
		MutablePoints.Add(InPoints[Cluster->Nodes[VtxIndex].PointIndex]);
		LastIndex = VtxIndex;
	}
	if (Context->bAddGoalToPath) { MutablePoints.Add_GetRef(PointIO->GetInPoint(PointIO->GetNum() - 1)).MetadataEntry = PCGInvalidEntryKey; }

	if (ExtraWeights != GlobalExtraWeights) { PCGEX_DELETE(ExtraWeights) }

	PathPoints.Tags->Append(PointIO->Tags);

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
