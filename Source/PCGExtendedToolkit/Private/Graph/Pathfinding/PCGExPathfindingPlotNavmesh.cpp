// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingPlotNavmesh.h"

#include "NavigationSystem.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExPathfindingPlotNavmeshElement"
#define PCGEX_NAMESPACE PathfindingPlotNavmesh

TArray<FPCGPinProperties> UPCGExPathfindingPlotNavmeshSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_OPERATION_OVERRIDES(PCGExDataBlending::SourceOverridesBlendingOps)
	return PinProperties;
}

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

PCGExData::EIOInit UPCGExPathfindingPlotNavmeshSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(PathfindingPlotNavmesh)

bool FPCGExPathfindingPlotNavmeshElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingPlotNavmesh)

	PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendOperation, PCGExDataBlending::SourceOverridesBlendingOps)

	Context->OutputPaths = MakeShared<PCGExData::FPointIOCollection>(Context);

	PCGEX_FWD(bAddSeedToPath)
	PCGEX_FWD(bAddGoalToPath)
	PCGEX_FWD(bAddPlotPointsToPath)

	PCGEX_FWD(NavAgentProperties)
	PCGEX_FWD(bRequireNavigableEndLocation)
	PCGEX_FWD(PathfindingMode)

	Context->FuseDistance = Settings->FuseDistance;

	return true;
}

bool FPCGExPathfindingPlotNavmeshElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathfindingPlotNavmeshElement::Execute);

	PCGEX_CONTEXT(PathfindingPlotNavmesh)
	PCGEX_EXECUTION_CHECK

	PCGEX_ON_INITIAL_EXECUTION
	{
		while (Context->AdvancePointsIO(false))
		{
			if (Context->CurrentIO->GetNum() < 2) { continue; }
			Context->GetAsyncManager()->Start<FPCGExPlotNavmeshTask>(-1, Context->CurrentIO);
		}
		Context->SetAsyncState(PCGEx::State_ProcessingPoints);
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGEx::State_ProcessingPoints)
	{
		Context->OutputPaths->StageOutputs();
		Context->Done();
	}

	return Context->TryComplete();
}

bool FPCGExPlotNavmeshTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
{
	FPCGExPathfindingPlotNavmeshContext* Context = AsyncManager->GetContext<FPCGExPathfindingPlotNavmeshContext>();
	PCGEX_SETTINGS(PathfindingPlotNavmesh)

	UWorld* World = Context->SourceComponent->GetWorld();
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);

	if (!NavSys || !NavSys->GetDefaultNavDataInstance()) { return false; }

	const int32 NumPlots = PointIO->GetNum();

	TArray<PCGExPathfinding::FPlotPoint> PathLocations;
	const FPCGPoint& FirstPoint = PointIO->GetInPoint(0);
	PathLocations.Emplace_GetRef(0, FirstPoint.Transform.GetLocation(), FirstPoint.MetadataEntry);
	FVector LastPosition = FVector::ZeroVector;

	//int32 MaxIterations = Settings->bClosedLoop ? NumPlots : NumPlots - 1;
	for (int i = 0; i < NumPlots - 1; i++)
	{
		FPCGPoint SeedPoint;
		FPCGPoint GoalPoint;

		if (Settings->bClosedLoop && i == NumPlots - 1)
		{
			SeedPoint = PointIO->GetInPoint(i);
			GoalPoint = PointIO->GetInPoint(0);
		}
		else
		{
			SeedPoint = PointIO->GetInPoint(i);
			GoalPoint = PointIO->GetInPoint(i + 1);
		}

		FVector SeedPosition = SeedPoint.Transform.GetLocation();
		FVector GoalPosition = GoalPoint.Transform.GetLocation();

		bool bAddGoal = Context->bAddPlotPointsToPath && i != NumPlots - 2;
		///

		FPathFindingQuery PathFindingQuery = FPathFindingQuery(
			World, *NavSys->GetDefaultNavDataInstance(),
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
		else if (Settings->bOmitCompletePathOnFailedPlot)
		{
			return false;
		}
		else if (bAddGoal)
		{
			PathLocations.Emplace_GetRef(i, GoalPosition, GoalPoint.MetadataEntry);
		}

		PathLocations.Last().PlotIndex = i + 1;
	}

	if (Settings->bClosedLoop)
	{
		const FPCGPoint& LastPoint = PointIO->GetInPoint(0);
		PathLocations.Emplace_GetRef(0, LastPoint.Transform.GetLocation(), LastPoint.MetadataEntry);
	}
	else
	{
		const FPCGPoint& LastPoint = PointIO->GetInPoint(NumPlots - 1);
		PathLocations.Emplace_GetRef(NumPlots - 1, LastPoint.Transform.GetLocation(), LastPoint.MetadataEntry);
	}


	int32 LastPlotIndex = -1;
	TArray<int32> Milestones;
	TArray<PCGExPaths::FPathMetrics> MilestonesMetrics;

	PCGExPaths::FPathMetrics* CurrentMetrics = nullptr;

	PCGExPaths::FPathMetrics Metrics = PCGExPaths::FPathMetrics(PathLocations[0].Position);
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
	if (PathLocations.Num() < 2) { return false; }

	const int32 NumPositions = PathLocations.Num();

	TSharedPtr<PCGExData::FPointIO> PathIO = Context->OutputPaths->Emplace_GetRef(PointIO, PCGExData::EIOInit::NewOutput);
	TSharedPtr<PCGExData::FFacade> PathDataFacade = MakeShared<PCGExData::FFacade>(PathIO.ToSharedRef());

	UPCGPointData* OutData = PathIO->GetOut();
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

	TSharedPtr<PCGExDataBlending::FMetadataBlender> TempBlender =
		Context->Blending->CreateBlender(PathDataFacade.ToSharedRef(), PathDataFacade.ToSharedRef(), PCGExData::ESource::Out);

	for (int i = 0; i < Milestones.Num() - 1; i++)
	{
		int32 StartIndex = Milestones[i] - 1;
		int32 EndIndex = Milestones[i + 1] + 1;
		int32 Range = EndIndex - StartIndex - 1;

		const FPCGPoint* EndPoint = PathIO->TryGetOutPoint(EndIndex);
		if (!EndPoint) { continue; }

		const FPCGPoint& StartPoint = PathIO->GetOutPoint(StartIndex);

		TArrayView<FPCGPoint> View = MakeArrayView(MutablePoints.GetData() + StartIndex, Range);
		Context->Blending->BlendSubPoints(
			PCGExData::FPointRef(StartPoint, StartIndex), PCGExData::FPointRef(EndPoint, EndIndex),
			View, MilestonesMetrics[i], TempBlender.Get(), StartIndex);
	}

	PathDataFacade->Write(ManagerPtr.Pin());
	MilestonesMetrics.Empty();

	if (!Context->bAddSeedToPath) { MutablePoints.RemoveAt(0); }
	if (!Context->bAddGoalToPath) { MutablePoints.Pop(); }

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
