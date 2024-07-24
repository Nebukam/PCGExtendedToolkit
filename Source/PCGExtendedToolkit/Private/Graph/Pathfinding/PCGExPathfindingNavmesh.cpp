// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingNavmesh.h"

#include "NavigationSystem.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "PCGExPathfinding.cpp"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExPathfindingNavmeshElement"
#define PCGEX_NAMESPACE PathfindingNavmesh

TArray<FPCGPinProperties> UPCGExPathfindingNavmeshSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINT(PCGExGraph::SourceSeedsLabel, "Seeds points for pathfinding.", Required, {})
	PCGEX_PIN_POINT(PCGExGraph::SourceGoalsLabel, "Goals points for pathfinding.", Required, {})
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

PCGExData::EInit UPCGExPathfindingNavmeshSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(PathfindingNavmesh)

FPCGExPathfindingNavmeshContext::~FPCGExPathfindingNavmeshContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE_FACADE_AND_SOURCE(SeedsDataFacade)
	PCGEX_DELETE_FACADE_AND_SOURCE(GoalsDataFacade)
	PCGEX_DELETE_TARRAY(PathQueries)
	PCGEX_DELETE(OutputPaths)

	SeedAttributesToPathTags.Cleanup();
	GoalAttributesToPathTags.Cleanup();

	PCGEX_DELETE(SeedForwardHandler)
	PCGEX_DELETE(GoalForwardHandler)
}

bool FPCGExPathfindingNavmeshElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingNavmesh)

	PCGEX_OPERATION_BIND(GoalPicker, UPCGExGoalPickerRandom)
	PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendInterpolate)

	PCGExData::FPointIO* SeedsPoints = nullptr;
	PCGExData::FPointIO* GoalsPoints = nullptr;

	auto Exit = [&]()
	{
		PCGEX_DELETE(SeedsPoints)
		PCGEX_DELETE(GoalsPoints)
		return false;
	};

	SeedsPoints = PCGExData::TryGetSingleInput(Context, PCGExGraph::SourceSeedsLabel, true);
	if (!SeedsPoints) { return Exit(); }

	Context->SeedsDataFacade = new PCGExData::FFacade(SeedsPoints);

	GoalsPoints = PCGExData::TryGetSingleInput(Context, PCGExGraph::SourceGoalsLabel, true);
	if (!GoalsPoints) { return Exit(); }

	Context->GoalsDataFacade = new PCGExData::FFacade(GoalsPoints);

	PCGEX_FWD(SeedAttributesToPathTags)
	PCGEX_FWD(GoalAttributesToPathTags)

	if (!Context->SeedAttributesToPathTags.Init(Context, Context->SeedsDataFacade)) { return false; }
	if (!Context->GoalAttributesToPathTags.Init(Context, Context->GoalsDataFacade)) { return false; }

	Context->SeedForwardHandler = Settings->SeedForwarding.GetHandler(Context->SeedsDataFacade);
	Context->GoalForwardHandler = Settings->GoalForwarding.GetHandler(Context->GoalsDataFacade);

	Context->FuseDistance = Settings->FuseDistance * Settings->FuseDistance;

	Context->OutputPaths = new PCGExData::FPointIOCollection(Context);
	Context->OutputPaths->DefaultOutputLabel = PCGExGraph::OutputPathsLabel;

	// Prepare path queries

	Context->GoalPicker->PrepareForData(Context->SeedsDataFacade, Context->GoalsDataFacade);
	PCGExPathfinding::ProcessGoals(
		Context->SeedsDataFacade, Context->GoalPicker,
		[&](const int32 SeedIndex, const int32 GoalIndex)
		{
			Context->PathQueries.Add(
				new PCGExPathfinding::FPathQuery(
					SeedIndex, SeedsPoints->GetInPoint(SeedIndex).Transform.GetLocation(),
					GoalIndex, GoalsPoints->GetInPoint(GoalIndex).Transform.GetLocation()));
		});

	return true;
}

bool FPCGExPathfindingNavmeshElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathfindingNavmeshElement::Execute);

	PCGEX_CONTEXT(PathfindingNavmesh)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ProcessingPoints);
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		auto NavClusterTask = [&](const int32 SeedIndex, const int32 GoalIndex)
		{
			const int32 PathIndex = Context->PathQueries.Add(
				new PCGExPathfinding::FPathQuery(
					SeedIndex, Context->SeedsDataFacade->Source->GetInPoint(SeedIndex).Transform.GetLocation(),
					GoalIndex, Context->GoalsDataFacade->Source->GetInPoint(GoalIndex).Transform.GetLocation()));

			Context->GetAsyncManager()->Start<FSampleNavmeshTask>(PathIndex, Context->SeedsDataFacade->Source, &Context->PathQueries);
		};

		PCGExPathfinding::ProcessGoals(Context->SeedsDataFacade, Context->GoalPicker, NavClusterTask);
		Context->SetAsyncState(PCGExGraph::State_Pathfinding);
	}

	if (Context->IsState(PCGExGraph::State_Pathfinding))
	{
		PCGEX_ASYNC_WAIT

		Context->OutputPaths->OutputToContext();
		Context->Done();
	}

	return Context->TryComplete();
}

bool FSampleNavmeshTask::ExecuteTask()
{
	FPCGExPathfindingNavmeshContext* Context = static_cast<FPCGExPathfindingNavmeshContext*>(Manager->Context);
	PCGEX_SETTINGS(PathfindingNavmesh)

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Context->World);

	if (!NavSys || !NavSys->GetDefaultNavDataInstance()) { return false; }

	PCGExPathfinding::FPathQuery* Query = (*Queries)[TaskIndex];

	const FPCGPoint* Seed = Context->SeedsDataFacade->Source->TryGetInPoint(Query->SeedIndex);
	const FPCGPoint* Goal = Context->GoalsDataFacade->Source->TryGetInPoint(Query->GoalIndex);

	if (!Seed || !Goal) { return false; }

	FPathFindingQuery PathFindingQuery = FPathFindingQuery(
		Context->World, *NavSys->GetDefaultNavDataInstance(),
		Query->SeedPosition, Query->GoalPosition, nullptr, nullptr,
		TNumericLimits<FVector::FReal>::Max(),
		Context->bRequireNavigableEndLocation);

	PathFindingQuery.NavAgentProperties = Context->NavAgentProperties;

	const FPathFindingResult Result = NavSys->FindPathSync(
		Context->NavAgentProperties, PathFindingQuery,
		Context->PathfindingMode == EPCGExPathfindingNavmeshMode::Regular ? EPathFindingMode::Type::Regular : EPathFindingMode::Type::Hierarchical);

	if (Result.Result != ENavigationQueryResult::Type::Success) { return false; } ///


	const TArray<FNavPathPoint>& Points = Result.Path->GetPathPoints();

	TArray<FVector> PathLocations;
	PathLocations.Reserve(Points.Num());

	PathLocations.Add(Query->SeedPosition);
	for (const FNavPathPoint& PathPoint : Points) { PathLocations.Add(PathPoint.Location); }
	PathLocations.Add(Query->GoalPosition);

	PCGExMath::FPathMetricsSquared Metrics = PCGExMath::FPathMetricsSquared(PathLocations[0]);
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

	if (PathLocations.Num() <= 2) { return false; }

	const int32 NumPositions = PathLocations.Num();
	const int32 LastPosition = NumPositions - 1;

	PCGExData::FPointIO* PathIO = Context->OutputPaths->Emplace_GetRef(PointIO, PCGExData::EInit::NewOutput);
	PCGExData::FFacade* PathDataFacade = new PCGExData::FFacade(PathIO);

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

	PCGExDataBlending::FMetadataBlender* TempBlender =
		Context->Blending->CreateBlender(PathDataFacade, Context->GoalsDataFacade);

	TArrayView<FPCGPoint> View(MutablePoints);
	Context->Blending->BlendSubPoints(View, Metrics, TempBlender);
	PCGEX_DELETE(TempBlender)

	if (!Settings->bAddSeedToPath) { MutablePoints.RemoveAt(0); }
	if (!Settings->bAddGoalToPath) { MutablePoints.Pop(); }

	Context->SeedAttributesToPathTags.Tag(Query->SeedIndex, PathIO);
	Context->GoalAttributesToPathTags.Tag(Query->GoalIndex, PathIO);

	Context->SeedForwardHandler->Forward(Query->SeedIndex, PathDataFacade);
	Context->GoalForwardHandler->Forward(Query->GoalIndex, PathDataFacade);

	PathDataFacade->Write(Manager, true);
	PCGEX_DELETE(PathDataFacade)

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
