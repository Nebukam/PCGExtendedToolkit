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
	Plot.Reserve(PointIO->GetNum());
	for (const FPCGPoint& PlotPoint : PointIO->GetIn()->GetPoints()) { Plot.Add(PlotPoint.Transform.GetLocation()); }

	FVector PrevPosition = Plot.Pop();

	TArray<FVector> PathLocations;
	PathLocations.Add(PrevPosition);

	TArray<int32> PathIndices;
	PathIndices.Add(0);

	while (!Plot.IsEmpty())
	{
		int32 PlotIndex = Plot.Num();
		FVector GoalPosition = Plot.Pop();

		if (Context->bAddPlotPointsToPath && !Plot.IsEmpty())
		{
			PathLocations.Add(GoalPosition);
			PathIndices.Add(PlotIndex);
		}
		///

		FPathFindingQuery PathFindingQuery = FPathFindingQuery(
			Context->World, *Context->NavData,
			PrevPosition, GoalPosition, nullptr, nullptr,
			TNumericLimits<FVector::FReal>::Max(),
			Context->bRequireNavigableEndLocation);

		PathFindingQuery.NavAgentProperties = Context->NavAgentProperties;

		const FPathFindingResult Result = NavSys->FindPathSync(
			Context->NavAgentProperties, PathFindingQuery,
			Context->PathfindingMode == EPCGExPathfindingPlotNavmeshMode::Regular ? EPathFindingMode::Type::Regular : EPathFindingMode::Type::Hierarchical);

		if (Result.Result != ENavigationQueryResult::Type::Success) { continue; } ///

		for (const FNavPathPoint& PathPoint : Result.Path->GetPathPoints())
		{
			PathLocations.Add(PathPoint.Location);
			PathIndices.Add(PlotIndex);
		}
		///

		PrevPosition = GoalPosition;
	}

	PathLocations.Add(PrevPosition); // End point

	PCGExMath::FPathMetrics Metrics = PCGExMath::FPathMetrics(PathLocations[0]);
	int32 FuseCountReduce = Context->bAddGoalToPath ? 2 : 1;
	for (int i = Context->bAddSeedToPath; i < PathLocations.Num(); i++)
	{
		FVector CurrentLocation = PathLocations[i];
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

	for (int i = 0; i < NumPositions; i++)
	{
		(MutablePoints[i] = PointIO->GetInPoint(PathIndices[i])).Transform.SetLocation(PathLocations[i]);
	}

	const PCGExDataBlending::FMetadataBlender* TempBlender = Context->Blending->CreateBlender(
		OutData, PointIO->GetIn(),
		PathPoints.CreateOutKeys(), PointIO->GetInKeys());

	int32 LastPlotPoint = PathIndices.Pop();
	int32 ViewLength = 0;

	while (!PathIndices.IsEmpty())
	{
		ViewLength++;
		int32 NewPlotPoint = PathIndices.Pop();
		if (NewPlotPoint == LastPlotPoint) { continue; }

		TArrayView<FPCGPoint> View = MakeArrayView(MutablePoints.GetData() + LastPlotPoint, ViewLength);
		Context->Blending->BlendSubPoints(View, Metrics, TempBlender, LastPlotPoint);

		LastPlotPoint = NewPlotPoint;
	}

	PCGEX_DELETE(TempBlender)

	if (!Context->bAddSeedToPath) { MutablePoints.RemoveAt(0); }
	if (!Context->bAddGoalToPath) { MutablePoints.Pop(); }

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
