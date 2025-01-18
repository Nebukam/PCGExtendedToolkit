// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingNavmesh.h"

#include "NavigationSystem.h"
#include "PCGExPointsProcessor.h"


#include "Graph/PCGExGraph.h"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExPathfindingNavmeshElement"
#define PCGEX_NAMESPACE PathfindingNavmesh

TArray<FPCGPinProperties> UPCGExPathfindingNavmeshSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINT(PCGExGraph::SourceSeedsLabel, "Seeds points for pathfinding.", Required, {})
	PCGEX_PIN_POINT(PCGExGraph::SourceGoalsLabel, "Goals points for pathfinding.", Required, {})
	PCGEX_PIN_OPERATION_OVERRIDES(PCGExPathfinding::SourceOverridesGoalPicker)
	PCGEX_PIN_OPERATION_OVERRIDES(PCGExDataBlending::SourceOverridesBlendingOps)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPathfindingNavmeshSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExGraph::OutputPathsLabel, "Paths output.", Required, {})
	return PinProperties;
}

#if WITH_EDITOR
void UPCGExPathfindingNavmeshSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (GoalPicker) { GoalPicker->UpdateUserFacingInfos(); }
	if (Blending) { Blending->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGExData::EIOInit UPCGExPathfindingNavmeshSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

PCGEX_INITIALIZE_ELEMENT(PathfindingNavmesh)

bool FPCGExPathfindingNavmeshElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingNavmesh)

	PCGEX_OPERATION_BIND(GoalPicker, UPCGExGoalPicker, PCGExPathfinding::SourceOverridesGoalPicker)
	PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendOperation, PCGExDataBlending::SourceOverridesBlendingOps)

	Context->SeedsDataFacade = PCGExData::TryGetSingleFacade(Context, PCGExGraph::SourceSeedsLabel, true);
	Context->GoalsDataFacade = PCGExData::TryGetSingleFacade(Context, PCGExGraph::SourceGoalsLabel, true);

	if (!Context->SeedsDataFacade || !Context->GoalsDataFacade) { return false; }

	Context->SeedsDataFacade = MakeShared<PCGExData::FFacade>(Context->SeedsDataFacade->Source);
	Context->GoalsDataFacade = MakeShared<PCGExData::FFacade>(Context->GoalsDataFacade->Source);

	PCGEX_FWD(SeedAttributesToPathTags)
	PCGEX_FWD(GoalAttributesToPathTags)

	if (!Context->SeedAttributesToPathTags.Init(Context, Context->SeedsDataFacade)) { return false; }
	if (!Context->GoalAttributesToPathTags.Init(Context, Context->GoalsDataFacade)) { return false; }

	Context->SeedForwardHandler = Settings->SeedForwarding.GetHandler(Context->SeedsDataFacade);
	Context->GoalForwardHandler = Settings->GoalForwarding.GetHandler(Context->GoalsDataFacade);

	Context->FuseDistance = Settings->FuseDistance;

	Context->OutputPaths = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->OutputPaths->OutputPin = PCGExGraph::OutputPathsLabel;

	// Prepare path queries

	if (!Context->GoalPicker->PrepareForData(Context, Context->SeedsDataFacade, Context->GoalsDataFacade)) { return false; }

	PCGExPathfinding::ProcessGoals(
		Context->SeedsDataFacade, Context->GoalPicker,
		[&](const int32 SeedIndex, const int32 GoalIndex)
		{
			Context->PathQueries.Emplace(
				SeedIndex, Context->SeedsDataFacade->Source->GetInPoint(SeedIndex).Transform.GetLocation(),
				GoalIndex, Context->GoalsDataFacade->Source->GetInPoint(GoalIndex).Transform.GetLocation());
		});

	if (Context->PathQueries.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Could not generate any queries."));
		return false;
	}

	return true;
}

bool FPCGExPathfindingNavmeshElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathfindingNavmeshElement::Execute);

	PCGEX_CONTEXT(PathfindingNavmesh)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		const TSharedPtr<PCGExMT::FTaskManager> AsyncManager = Context->GetAsyncManager();
		auto NavClusterTask = [&](const int32 SeedIndex, const int32 GoalIndex)
		{
			const int32 PathIndex = Context->PathQueries.Emplace(
				SeedIndex, Context->SeedsDataFacade->Source->GetInPoint(SeedIndex).Transform.GetLocation(),
				GoalIndex, Context->GoalsDataFacade->Source->GetInPoint(GoalIndex).Transform.GetLocation());

			PCGEX_LAUNCH(FSampleNavmeshTask, PathIndex, Context->SeedsDataFacade->Source, &Context->PathQueries)
		};

		PCGExPathfinding::ProcessGoals(Context->SeedsDataFacade, Context->GoalPicker, NavClusterTask);
		Context->SetAsyncState(PCGExGraph::State_Pathfinding);
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGExGraph::State_Pathfinding)
	{
		Context->OutputPaths->StageOutputs();
		Context->Done();
	}

	return Context->TryComplete();
}

void FSampleNavmeshTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
{
	FPCGExPathfindingNavmeshContext* Context = AsyncManager->GetContext<FPCGExPathfindingNavmeshContext>();
	PCGEX_SETTINGS(PathfindingNavmesh)

	UWorld* World = Context->SourceComponent->GetWorld();
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);

	if (!NavSys || !NavSys->GetDefaultNavDataInstance()) { return; }

	PCGExPathfinding::FSeedGoalPair Query = (*Queries)[TaskIndex];

	const FPCGPoint* Seed = Context->SeedsDataFacade->Source->TryGetInPoint(Query.Seed);
	const FPCGPoint* Goal = Context->GoalsDataFacade->Source->TryGetInPoint(Query.Goal);

	if (!Seed || !Goal) { return; }

	FPathFindingQuery PathFindingQuery = FPathFindingQuery(
		World, *NavSys->GetDefaultNavDataInstance(),
		Query.SeedPosition, Query.GoalPosition, nullptr, nullptr,
		TNumericLimits<FVector::FReal>::Max(),
		Context->bRequireNavigableEndLocation);

	PathFindingQuery.NavAgentProperties = Context->NavAgentProperties;

	const FPathFindingResult Result = NavSys->FindPathSync(
		Context->NavAgentProperties, PathFindingQuery,
		Context->PathfindingMode == EPCGExPathfindingNavmeshMode::Regular ? EPathFindingMode::Type::Regular : EPathFindingMode::Type::Hierarchical);

	if (Result.Result != ENavigationQueryResult::Type::Success) { return; } ///


	const TArray<FNavPathPoint>& Points = Result.Path->GetPathPoints();

	TArray<FVector> PathLocations;
	PathLocations.Reserve(Points.Num());

	PathLocations.Add(Query.SeedPosition);
	for (const FNavPathPoint& PathPoint : Points) { PathLocations.Add(PathPoint.Location); }
	PathLocations.Add(Query.GoalPosition);

	PCGExPaths::FPathMetrics Metrics = PCGExPaths::FPathMetrics(PathLocations[0]);
	int32 FuseCountReduce = Settings->bAddGoalToPath ? 2 : 1;
	for (int i = Settings->bAddSeedToPath; i < PathLocations.Num(); i++)
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

	if (PathLocations.Num() <= 2) { return; }

	const int32 NumPositions = PathLocations.Num();
	const int32 LastPosition = NumPositions - 1;

	TSharedPtr<PCGExData::FPointIO> PathIO = Context->OutputPaths->Emplace_GetRef(PointIO, PCGExData::EIOInit::New);
	PCGEX_MAKE_SHARED(PathDataFacade, PCGExData::FFacade, PathIO.ToSharedRef())

	UPCGPointData* OutData = PathIO->GetOut();
	TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();
	MutablePoints.SetNumUninitialized(NumPositions);

	FVector Location;
	for (int i = 0; i < LastPosition; i++)
	{
		Location = PathLocations[i];
		(MutablePoints[i] = *Seed).Transform.SetLocation(Location);
	}

	Location = PathLocations[LastPosition];
	(MutablePoints[LastPosition] = *Goal).Transform.SetLocation(Location);

	TSharedPtr<PCGExDataBlending::FMetadataBlender> TempBlender =
		Context->Blending->CreateBlender(PathDataFacade.ToSharedRef(), Context->GoalsDataFacade.ToSharedRef());

	Context->Blending->BlendSubPoints(MutablePoints, Metrics, TempBlender.Get());

	if (!Settings->bAddSeedToPath) { MutablePoints.RemoveAt(0); }
	if (!Settings->bAddGoalToPath) { MutablePoints.Pop(); }

	Context->SeedAttributesToPathTags.Tag(Query.Seed, PathIO);
	Context->GoalAttributesToPathTags.Tag(Query.Goal, PathIO);

	Context->SeedForwardHandler->Forward(Query.Seed, PathDataFacade);
	Context->GoalForwardHandler->Forward(Query.Goal, PathDataFacade);

	PathDataFacade->Write(AsyncManager);
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
