// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingPlotNavmesh.h"

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
	PCGEX_PIN_POINTS(PCGExPaths::OutputPathsLabel, "Paths output.", Required, {})
	return PinProperties;
}

#if WITH_EDITOR
void UPCGExPathfindingPlotNavmeshSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Blending) { Blending->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGEX_INITIALIZE_ELEMENT(PathfindingPlotNavmesh)

bool FPCGExPathfindingPlotNavmeshElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingPlotNavmesh)

	PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendInstancedFactory, PCGExDataBlending::SourceOverridesBlendingOps)

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
		const TSharedPtr<PCGExMT::FTaskManager> AsyncManager = Context->GetAsyncManager();
		while (Context->AdvancePointsIO(false))
		{
			if (Context->CurrentIO->GetNum() < 2) { continue; }
			PCGEX_LAUNCH(FPCGExPlotNavmeshTask, Context->CurrentIO)
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

void FPCGExPlotNavmeshTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
{
	FPCGExPathfindingPlotNavmeshContext* Context = AsyncManager->GetContext<FPCGExPathfindingPlotNavmeshContext>();
	PCGEX_SETTINGS(PathfindingPlotNavmesh)

	const int32 NumPlots = PointIO->GetNum();

	TArray<PCGExNavmesh::FNavmeshQuery> PlotQueries;
	PlotQueries.Reserve(NumPlots);

	auto PlotQuery = [&](const int32 SeedIndex, const int32 GoalIndex)-> PCGExNavmesh::FNavmeshQuery& {
		PCGExNavmesh::FNavmeshQuery& Query = PlotQueries.Emplace_GetRef(PCGExPathfinding::FSeedGoalPair(PointIO->GetInPoint(SeedIndex), PointIO->GetInPoint(GoalIndex)));
		Query.FindPath(Context);
		return Query;
	};


	PCGEx::FRWScope PlotScope(NumPlots + 2, false);

	int32 FinalNumPoints = 0;

	for (int i = 0; i < NumPlots - 1; i++)
	{
		PCGExNavmesh::FNavmeshQuery& Query = PlotQuery(i, i + 1);

		if (i == 0 && Settings->bAddSeedToPath)
		{
			// First query and we want seed in the mix
			PlotScope.Add(Query.SeedGoalPair.Seed, FinalNumPoints++);
		}

		FinalNumPoints += Query.Positions.Num();

		if (Settings->bAddPlotPointsToPath || (i == NumPlots - 2 && !Settings->bClosedLoop && Settings->bAddGoalToPath))
		{
			// Either last query & we want goals,
			// or we want plot points, in which case we insert goals (since any non-last goal is the next query' seed)
			PlotScope.Add(Query.SeedGoalPair.Goal, FinalNumPoints++);
		}
	}

	if (Settings->bClosedLoop)
	{
		PCGExNavmesh::FNavmeshQuery& Query = PlotQuery(NumPlots - 1, 0);
		FinalNumPoints += Query.Positions.Num();
		// No extras, it's a wrapping path
	}


	// TODO : Proper refactor.
	// A --- B --- C
	// A --- B --- C --- A(omit)

	/*

	//int32 MaxIterations = Settings->bClosedLoop ? NumPlots : NumPlots - 1;
	for (int i = 0; i < NumPlots - 1; i++)
	{
		const int32 SeedIndex = i;
		const int32 GoalIndex = Settings->bClosedLoop && i == NumPlots - 1 ? 0 : i + 1;

		FVector SeedPosition = PlotTransforms[SeedIndex].GetLocation();
		FVector GoalPosition = PlotTransforms[GoalIndex].GetLocation();

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
				PathLocations.Emplace(SeedIndex, PathPoint.Location);
			}

			LastPosition = PathLocations.Last().Position;
			if (bAddGoal) { PathLocations.Emplace(GoalIndex, GoalPosition); }
		}
		else if (Settings->bOmitCompletePathOnFailedPlot)
		{
			return;
		}
		else if (bAddGoal)
		{
			PathLocations.Emplace(SeedIndex, GoalPosition);
		}

		PathLocations.Last().PlotIndex = GoalIndex;
	}

	if (Settings->bClosedLoop)
	{
		PathLocations.Emplace(0, PlotTransforms[0].GetLocation());
	}
	else
	{
		const int32 LastIndex = NumPlots - 1;
		PathLocations.Emplace(LastIndex, PlotTransforms[LastIndex].GetLocation());
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

	if (PathLocations.Num() <= PointIO->GetNum()) { return; } //
	if (PathLocations.Num() < 2) { return; }

	const int32 NumPositions = PathLocations.Num();

	TSharedPtr<PCGExData::FPointIO> PathIO = Context->OutputPaths->Emplace_GetRef(PointIO, PCGExData::EIOInit::New);
	PCGEX_MAKE_SHARED(PathDataFacade, PCGExData::FFacade, PathIO.ToSharedRef())

	UPCGBasePointData* OutPathData = PathIO->GetOut();
	OutPathData->SetNumPoints(NumPositions);

	TConstPCGValueRange<FTransform> InTransforms = PointIO->GetIn()->GetConstTransformValueRange();
	TPCGValueRange<FTransform> OutTransforms = OutPathData->GetTransformValueRange();
	TPCGValueRange<int64> OutMetadataEntries = OutPathData->GetMetadataEntryValueRange();

	for (int i = 0; i < NumPositions; i++)
	{
		PCGExPathfinding::FPlotPoint PPoint = PathLocations[i];
		OutTransforms[i].SetLocation(InTransforms[PPoint.PlotIndex].GetLocation());
		OutMetadataEntries[i] = PPoint.MetadataEntryKey;
	}

	PathLocations.Empty();

	TSharedPtr<FPCGExSubPointsBlendOperation> SubBlending = Context->Blending->CreateOperation();
	if (!SubBlending->PrepareForData(Context, PathDataFacade)) { return; }

	const int32 MaxIndex = OutPathData->GetNumPoints() - 1;

	for (int i = 0; i < Milestones.Num() - 1; i++)
	{
		int32 StartIndex = Milestones[i] - 1;
		int32 EndIndex = Milestones[i + 1] + 1;
		int32 Range = EndIndex - StartIndex - 1;

		if (EndIndex < 0 || EndIndex > MaxIndex) { continue; }

		PCGExData::FConstPoint StartPoint(OutPathData, StartIndex);
		PCGExData::FConstPoint EndPoint(OutPathData, EndIndex);

		TArrayView<FPCGPoint> View = MakeArrayView(MutablePoints.GetData() + StartIndex, Range);
		SubBlending->BlendSubPoints(StartPoint, EndPoint, View, MilestonesMetrics[i], TempBlender.Get(), StartIndex);
	}

	PathDataFacade->Write(AsyncManager);
	MilestonesMetrics.Empty();

	// TODO : 

	if (!Context->bAddSeedToPath) { MutablePoints.RemoveAt(0); }
	if (!Context->bAddGoalToPath) { MutablePoints.Pop(); }

	*/
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
