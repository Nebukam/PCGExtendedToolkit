// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingPlotEdges.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "PCGExPathfinding.cpp"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Algo/Reverse.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDistance.h"

#define LOCTEXT_NAMESPACE "PCGExPathfindingPlotEdgesElement"
#define PCGEX_NAMESPACE PathfindingPlotEdges

UPCGExPathfindingPlotEdgesSettings::UPCGExPathfindingPlotEdgesSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PCGEX_OPERATION_DEFAULT(Heuristics, UPCGExHeuristicDistance)
}

void UPCGExPathfindingPlotEdgesSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Heuristics) { Heuristics->UpdateUserFacingInfos(); }
	HeuristicsModifiers.UpdateUserFacingInfos();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

TArray<FPCGPinProperties> UPCGExPathfindingPlotEdgesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	FPCGPinProperties& PinPropertySeeds = PinProperties.Emplace_GetRef(PCGExPathfinding::SourcePlotsLabel, EPCGDataType::Point, true, true);

#if WITH_EDITOR
	PinPropertySeeds.Tooltip = FTEXT("Plot points for pathfinding.");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPathfindingPlotEdgesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PinProperties.Pop(); // Remove edge output
	return PinProperties;
}

FPCGElementPtr UPCGExPathfindingPlotEdgesSettings::CreateElement() const { return MakeShared<FPCGExPathfindingPlotEdgesElement>(); }

FPCGExPathfindingPlotEdgesContext::~FPCGExPathfindingPlotEdgesContext()
{
	PCGEX_TERMINATE_ASYNC

	if (HeuristicsModifiers) { HeuristicsModifiers->Cleanup(); }

	PCGEX_DELETE(Plots)
	PCGEX_DELETE(OutputPaths)
}

PCGEX_INITIALIZE_CONTEXT(PathfindingPlotEdges)

bool FPCGExPathfindingPlotEdgesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingPlotEdges)

	PCGEX_OPERATION_BIND(Heuristics, UPCGExHeuristicDistance)

	Context->OutputPaths = new PCGExData::FPointIOGroup();
	Context->Plots = new PCGExData::FPointIOGroup();

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExPathfinding::SourcePlotsLabel);
	Context->Plots->Initialize(InContext, Sources, PCGExData::EInit::NoOutput);

	if (Context->Plots->IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing Plots Points."));
		return false;
	}

	Context->HeuristicsModifiers = const_cast<FPCGExHeuristicModifiersSettings*>(&Settings->HeuristicsModifiers);

	return true;
}

bool FPCGExPathfindingPlotEdgesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathfindingPlotEdgesElement::Execute);

	PCGEX_CONTEXT(PathfindingPlotEdges)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvanceAndBindPointsIO()) { Context->Done(); }
		else
		{
			if (!Context->BoundEdges->IsValid())
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
		if (!Context->AdvanceEdges()) { Context->SetState(PCGExMT::State_ReadyForNextPoints); }
		else
		{
			if (Context->CurrentMesh->HasInvalidEdges())
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input edges are invalid. This will highly likely cause unexpected results or failed pathfinding."));
			}
			Context->HeuristicsModifiers->PrepareForData(*Context->CurrentIO, *Context->CurrentEdges, Context->Heuristics->GetScale());
			Context->SetState(PCGExGraph::State_ProcessingEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		Context->Plots->ForEach(
			[&](PCGExData::FPointIO& PlotIO, const int32 Index)
			{
				if (PlotIO.GetNum() < 2) { return; }
				Context->GetAsyncManager()->Start<FPlotMeshPathTask>(Index, &PlotIO);
			});

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (Context->IsAsyncWorkComplete()) { Context->SetState(PCGExGraph::State_ReadyForNextEdges); }
	}

	if (Context->IsDone()) { Context->OutputPaths->OutputTo(Context, true); }

	return Context->IsDone();
}

bool FPlotMeshPathTask::ExecuteTask()
{
	FPCGExPathfindingPlotEdgesContext* Context = Manager->GetContext<FPCGExPathfindingPlotEdgesContext>();
	//PCGEX_ASYNC_CHECKPOINT

	const PCGExMesh::FMesh* Mesh = Context->CurrentMesh;
	TArray<int32> Path;

	const int32 NumPlots = PointIO->GetNum();

	for (int i = 1; i < NumPlots; i++)
	{
		FVector SeedPosition = PointIO->GetInPoint(i - 1).Transform.GetLocation();
		FVector GoalPosition = PointIO->GetInPoint(i).Transform.GetLocation();

		//Note: Can silently fail
		PCGExPathfinding::FindPath(
			Context->CurrentMesh, SeedPosition, GoalPosition,
			Context->Heuristics, Context->HeuristicsModifiers, Path);

		if (Context->bAddPlotPointsToPath && i < NumPlots - 1) { Path.Add((i + 1) * -1); }

		SeedPosition = GoalPosition;
	}

	const PCGExData::FPointIO& PathPoints = Context->OutputPaths->Emplace_GetRef(Context->GetCurrentIn(), PCGExData::EInit::NewOutput);
	UPCGPointData* OutData = PathPoints.GetOut();
	TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();
	const TArray<FPCGPoint>& InPoints = Context->GetCurrentIn()->GetPoints();

	MutablePoints.Reserve(Path.Num() + 2);

	if (Context->bAddSeedToPath) { MutablePoints.Add_GetRef(PointIO->GetInPoint(0)).MetadataEntry = PCGInvalidEntryKey; }
	int32 LastIndex = -1;
	for (const int32 VtxIndex : Path)
	{
		if (VtxIndex < -1) // Plot point
		{
			MutablePoints.Add_GetRef(PointIO->GetInPoint((VtxIndex * -1) - 1)).MetadataEntry = PCGInvalidEntryKey;
			continue;
		}

		if (LastIndex == VtxIndex) { continue; } //Skip duplicates
		MutablePoints.Add(InPoints[Mesh->Vertices[VtxIndex].PointIndex]);
		LastIndex = VtxIndex;
	}
	if (Context->bAddGoalToPath) { MutablePoints.Add_GetRef(PointIO->GetInPoint(PointIO->GetNum() - 1)).MetadataEntry = PCGInvalidEntryKey; }

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
