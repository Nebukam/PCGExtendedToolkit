// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingPlotNavmesh.h"

#include "NavigationSystem.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "PCGExPathfinding.cpp"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExPathfindingPlotNavmeshElement"
#define PCGEX_NAMESPACE PathfindingPlotNavmesh

TArray<FPCGPinProperties> UPCGExPathfindingPlotNavmeshSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExGraph::OutputPathsLabel, "Paths output.", Required, {})
	return PinProperties;
}

#if WITH_EDITOR
void UPCGExPathfindingPlotNavmeshSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Blending) { Blending->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGExData::EInit UPCGExPathfindingPlotNavmeshSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FName UPCGExPathfindingPlotNavmeshSettings::GetMainInputLabel() const { return PCGExPathfinding::SourcePlotsLabel; }
FName UPCGExPathfindingPlotNavmeshSettings::GetMainOutputLabel() const { return PCGExGraph::OutputPathsLabel; }

PCGEX_INITIALIZE_ELEMENT(PathfindingPlotNavmesh)

void UPCGExPathfindingPlotNavmeshSettings::PostInitProperties()
{
	Super::PostInitProperties();
	PCGEX_OPERATION_DEFAULT(Blending, UPCGExSubPointsBlendInterpolate)
}

FPCGExPathfindingPlotNavmeshContext::~FPCGExPathfindingPlotNavmeshContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(OutputPaths)
}


bool FPCGExPathfindingPlotNavmeshElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingPlotNavmesh)

	PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendInterpolate)

	Context->OutputPaths = new PCGExData::FPointIOCollection();

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
			Context->GetAsyncManager()->Start<FPCGExPlotNavmeshTask>(-1, Context->CurrentIO);
		}
		Context->SetAsyncState(PCGExMT::State_ProcessingPoints);
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		PCGEX_WAIT_ASYNC
		Context->Done();
	}

	if (Context->IsDone())
	{
		Context->OutputPaths->OutputTo(Context);
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}

bool FPCGExPlotNavmeshTask::ExecuteTask()
{
	FPCGExPathfindingPlotNavmeshContext* Context = static_cast<FPCGExPathfindingPlotNavmeshContext*>(Manager->Context);


	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Context->World);

	if (!NavSys || !NavSys->GetDefaultNavDataInstance()) { return false; }

	const int32 NumPlots = PointIO->GetNum();

	TArray<PCGExPathfinding::FPlotPoint> PathLocations;
	const FPCGPoint& FirstPoint = PointIO->GetInPoint(0);
	PathLocations.Emplace_GetRef(0, FirstPoint.Transform.GetLocation(), FirstPoint.MetadataEntry);
	FVector LastPosition = FVector::ZeroVector;

	for (int i = 0; i < NumPlots - 1; i++)
	{
		const FPCGPoint& SeedPoint = PointIO->GetInPoint(i);
		const FPCGPoint& GoalPoint = PointIO->GetInPoint(i + 1);

		FVector SeedPosition = SeedPoint.Transform.GetLocation();
		FVector GoalPosition = GoalPoint.Transform.GetLocation();

		bool bAddGoal = Context->bAddPlotPointsToPath && i != NumPlots - 2;
		///

		FPathFindingQuery PathFindingQuery = FPathFindingQuery(
			Context->World, *NavSys->GetDefaultNavDataInstance(),
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
				if (PathPoint.Location == LastPosition) { continue; } // When plotting, end from prev path == start from new path
				PathLocations.Emplace_GetRef(i, PathPoint.Location, PCGInvalidEntryKey);
			}

			LastPosition = PathLocations.Last().Position;

			if (bAddGoal) { PathLocations.Emplace_GetRef(i, GoalPosition, PCGInvalidEntryKey); }

			PathLocations.Last().MetadataEntryKey = GoalPoint.MetadataEntry;
		}
		else if (bAddGoal)
		{
			PathLocations.Emplace_GetRef(i, GoalPosition, GoalPoint.MetadataEntry);
		}

		PathLocations.Last().PlotIndex = i + 1;
	}

	const FPCGPoint& LastPoint = PointIO->GetInPoint(NumPlots - 1);
	PathLocations.Emplace_GetRef(NumPlots - 1, LastPoint.Transform.GetLocation(), LastPoint.MetadataEntry);

	int32 LastPlotIndex = -1;
	TArray<int32> Milestones;
	TArray<PCGExMath::FPathMetricsSquared> MilestonesMetrics;

	PCGExMath::FPathMetricsSquared* CurrentMetrics = nullptr;

	PCGExMath::FPathMetricsSquared Metrics = PCGExMath::FPathMetricsSquared(PathLocations[0].Position);
	int32 FuseCountReduce = Context->bAddGoalToPath ? 2 : 1;
	for (int i = Context->bAddSeedToPath; i < PathLocations.Num(); i++)
	{
		PCGExPathfinding::FPlotPoint PPoint = PathLocations[i];
		FVector CurrentLocation = PPoint.Position;

		if (LastPlotIndex != PPoint.PlotIndex)
		{
			LastPlotIndex = PPoint.PlotIndex;
			Milestones.Add(i);
			CurrentMetrics = &MilestonesMetrics.Emplace_GetRef(CurrentLocation);
		}
		else if (i > 0 && i < (PathLocations.Num() - FuseCountReduce) && PPoint.MetadataEntryKey == PCGInvalidEntryKey)
		{
			if (Metrics.IsLastWithinRange(CurrentLocation, Context->FuseDistance))
			{
				PathLocations.RemoveAt(i);
				i--;
				continue;
			}
		}

		Metrics.Add(CurrentLocation);
		CurrentMetrics->Add(CurrentLocation);
	}

	if (PathLocations.Num() <= PointIO->GetNum()) { return false; } //


	const int32 NumPositions = PathLocations.Num();

	PCGExData::FPointIO& PathPoints = Context->OutputPaths->Emplace_GetRef(*PointIO, PCGExData::EInit::NewOutput);
	UPCGPointData* OutData = PathPoints.GetOut();
	TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();

	MutablePoints.SetNumUninitialized(NumPositions);

	for (int i = 0; i < NumPositions; i++)
	{
		PCGExPathfinding::FPlotPoint PPoint = PathLocations[i];
		FPCGPoint& NewPoint = (MutablePoints[i] = PointIO->GetInPoint(PPoint.PlotIndex));
		NewPoint.Transform.SetLocation(PPoint.Position);
		NewPoint.MetadataEntry = PPoint.MetadataEntryKey;
	}
	PathLocations.Empty();

	PCGExDataBlending::FMetadataBlender* TempBlender =
		Context->Blending->CreateBlender(PathPoints, PathPoints, PCGExData::ESource::Out);

	for (int i = 0; i < Milestones.Num() - 1; i++)
	{
		int32 StartIndex = Milestones[i] - 1;
		int32 EndIndex = Milestones[i + 1] + 1;
		int32 Range = EndIndex - StartIndex - 1;

		const FPCGPoint* EndPoint = PathPoints.TryGetOutPoint(EndIndex);
		if (!EndPoint) { continue; }

		const FPCGPoint& StartPoint = PathPoints.GetOutPoint(StartIndex);

		TArrayView<FPCGPoint> View = MakeArrayView(MutablePoints.GetData() + StartIndex, Range);
		Context->Blending->BlendSubPoints(
			PCGEx::FPointRef(StartPoint, StartIndex), PCGEx::FPointRef(EndPoint, EndIndex),
			View, MilestonesMetrics[i], TempBlender);
	}

	TempBlender->Write();

	PCGEX_DELETE(TempBlender)
	MilestonesMetrics.Empty();

	if (!Context->bAddSeedToPath) { MutablePoints.RemoveAt(0); }
	if (!Context->bAddGoalToPath) { MutablePoints.Pop(); }

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
