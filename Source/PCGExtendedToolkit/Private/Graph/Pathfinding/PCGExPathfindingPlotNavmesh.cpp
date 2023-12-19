// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingPlotNavmesh.h"

#include "NavigationSystem.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "Graph/Pathfinding/PCGExPathfinding.h"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExPathfindingPlotNavmeshElement"
#define PCGEX_NAMESPACE PathfindingPlotNavmesh

UPCGExPathfindingPlotNavmeshSettings::UPCGExPathfindingPlotNavmeshSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PCGEX_DEFAULT_OPERATION(Blending, UPCGExSubPointsBlendInterpolate)
}

TArray<FPCGPinProperties> UPCGExPathfindingPlotNavmeshSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPathsOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputPathsLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPathsOutput.Tooltip = FTEXT("Paths output.");
#endif // WITH_EDITOR

	return PinProperties;
}

void UPCGExPathfindingPlotNavmeshSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Blending) { Blending->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

PCGExData::EInit UPCGExPathfindingPlotNavmeshSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }
int32 UPCGExPathfindingPlotNavmeshSettings::GetPreferredChunkSize() const { return 32; }

FName UPCGExPathfindingPlotNavmeshSettings::GetMainInputLabel() const { return PCGExPathfinding::SourcePlotsLabel; }
FName UPCGExPathfindingPlotNavmeshSettings::GetMainOutputLabel() const { return PCGExGraph::OutputPathsLabel; }

FPCGExPathfindingPlotNavmeshContext::~FPCGExPathfindingPlotNavmeshContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(OutputPaths)
}

FPCGElementPtr UPCGExPathfindingPlotNavmeshSettings::CreateElement() const { return MakeShared<FPCGExPathfindingPlotNavmeshElement>(); }

PCGEX_INITIALIZE_CONTEXT(PathfindingPlotNavmesh)

bool FPCGExPathfindingPlotNavmeshElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingPlotNavmesh)

	PCGEX_BIND_OPERATION(Blending, UPCGExSubPointsBlendInterpolate)

	if (!Settings->NavData)
	{
		if (const UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Context->World))
		{
			ANavigationData* NavData = NavSys->GetDefaultNavDataInstance();
			Context->NavData = NavData;
		}
	}

	if (!Context->NavData)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing Nav Data"));
		return false;
	}

	Context->OutputPaths = new PCGExData::FPointIOGroup();

	PCGEX_FWD(bAddSeedToPath)
	PCGEX_FWD(bAddGoalToPath)
	PCGEX_FWD(bAddPlotPointsToPath)

	PCGEX_FWD(NavAgentProperties)
	PCGEX_FWD(bRequireNavigableEndLocation)
	PCGEX_FWD(PathfindingMode)

	Context->FuseDistance = Settings->FuseDistance * Settings->FuseDistance;

	return true;
}

bool FPCGExPathfindingPlotNavmeshElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathfindingPlotNavmeshElement::Execute);

	PCGEX_CONTEXT(PathfindingPlotNavmesh)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		while (Context->AdvancePointsIO())
		{
			if (Context->CurrentIO->GetNum() < 2) { continue; }
			Context->GetAsyncManager()->Start<FPlotNavmeshTask>(-1, Context->CurrentIO);
		}
		Context->SetAsyncState(PCGExMT::State_ProcessingPoints);
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		if (Context->IsAsyncWorkComplete()) { Context->Done(); }
	}

	if (Context->IsDone())
	{
		Context->OutputPaths->OutputTo(Context, true);
	}

	return Context->IsDone();
}

bool FPlotNavmeshTask::ExecuteTask()
{
	FPCGExPathfindingPlotNavmeshContext* Context = static_cast<FPCGExPathfindingPlotNavmeshContext*>(Manager->Context);
	//PCGEX_ASYNC_CHECKPOINT

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Context->World);

	if (!NavSys) { return false; }

	TArray<FVector> Plot;
	int32 NumPlots = PointIO->GetNum();
	Plot.Reserve(NumPlots);
	for (const FPCGPoint& PlotPoint : PointIO->GetIn()->GetPoints()) { Plot.Add(PlotPoint.Transform.GetLocation()); }
	Algo::Reverse(Plot);

	int32 PlotIndex = 0;
	FVector SeedPosition = Plot.Pop();

	TArray<PCGExPathfinding::FPlotPoint> PathLocations;
	PathLocations.Emplace_GetRef(PlotIndex, SeedPosition);

	while (!Plot.IsEmpty())
	{
		FVector GoalPosition = Plot.Pop();

		///

		FPathFindingQuery PathFindingQuery = FPathFindingQuery(
			Context->World, *Context->NavData,
			SeedPosition, GoalPosition, nullptr, nullptr,
			TNumericLimits<FVector::FReal>::Max(),
			Context->bRequireNavigableEndLocation);

		PathFindingQuery.NavAgentProperties = Context->NavAgentProperties;

		const FPathFindingResult Result = NavSys->FindPathSync(
			Context->NavAgentProperties, PathFindingQuery,
			Context->PathfindingMode == EPCGExPathfindingNavmeshMode::Regular ? EPathFindingMode::Type::Regular : EPathFindingMode::Type::Hierarchical);

		if (Result.Result == ENavigationQueryResult::Type::Success)
		{
			for (const FNavPathPoint& PathPoint : Result.Path->GetPathPoints())
			{
				PathLocations.Emplace_GetRef(PlotIndex, PathPoint.Location);
			}
		}

		PlotIndex = NumPlots - Plot.Num() - 1;

		if (Context->bAddPlotPointsToPath && !Plot.IsEmpty()) { PathLocations.Emplace_GetRef(PlotIndex, GoalPosition); }

		SeedPosition = GoalPosition;
	}

	PathLocations.Emplace_GetRef(NumPlots-1, SeedPosition); // End point

	PCGExMath::FPathMetrics Metrics = PCGExMath::FPathMetrics(PathLocations[0].Position);
	int32 FuseCountReduce = Context->bAddGoalToPath ? 2 : 1;
	for (int i = Context->bAddSeedToPath; i < PathLocations.Num(); i++)
	{
		FVector CurrentLocation = PathLocations[i].Position;
		if (i > 0 && i < (PathLocations.Num() - FuseCountReduce))
		{
			if (Metrics.IsLastWithinRange(CurrentLocation, Context->FuseDistance))
			{
				PathLocations.RemoveAt(i);
				i--;
				continue;
			}
		}
		Metrics.Add(CurrentLocation);
	}

	if (PathLocations.Num() <= PointIO->GetNum()) { return false; } //
	//PCGEX_ASYNC_CHECKPOINT

	const int32 NumPositions = PathLocations.Num();

	PCGExData::FPointIO& PathPoints = Context->OutputPaths->Emplace_GetRef(*PointIO, PCGExData::EInit::NewOutput);
	UPCGPointData* OutData = PathPoints.GetOut();
	TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();
	MutablePoints.SetNumUninitialized(NumPositions);

	TArray<int32> BlendIndices;
	int32 LastBlendIndex = PathLocations[0].PlotIndex;
	BlendIndices.Add(0);
	for (int i = 0; i < NumPositions; i++)
	{
		PCGExPathfinding::FPlotPoint PPoint = PathLocations[i];
		(MutablePoints[i] = PointIO->GetInPoint(LastBlendIndex)).Transform.SetLocation(PPoint.Position);
		if (LastBlendIndex != PPoint.PlotIndex)
		{
			LastBlendIndex = PPoint.PlotIndex;
			BlendIndices.Add(i);
		}
	}
	PathLocations.Empty();

	const PCGExDataBlending::FMetadataBlender* TempBlender = Context->Blending->CreateBlender(
		OutData, PointIO->GetIn(),
		PathPoints.CreateOutKeys(), PointIO->GetInKeys());

	while (BlendIndices.Num() > 1)
	{
		int32 StartIndex = BlendIndices[0];
		int32 ViewLength = BlendIndices[1] - StartIndex;

		TArrayView<FPCGPoint> View = MakeArrayView(MutablePoints.GetData() + StartIndex, ViewLength);
		Context->Blending->BlendSubPoints(View, Metrics, TempBlender);

		BlendIndices.RemoveAt(0);
	}

	PCGEX_DELETE(TempBlender)

	if (!Context->bAddSeedToPath) { MutablePoints.RemoveAt(0); }
	if (!Context->bAddGoalToPath) { MutablePoints.Pop(); }

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
