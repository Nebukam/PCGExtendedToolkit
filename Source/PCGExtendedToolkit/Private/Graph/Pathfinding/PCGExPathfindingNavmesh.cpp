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
	PCGEX_PIN_POINT(PCGExPathfinding::SourceSeedsLabel, "Seeds points for pathfinding.", Required, {})
	PCGEX_PIN_POINT(PCGExPathfinding::SourceGoalsLabel, "Goals points for pathfinding.", Required, {})
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

FName UPCGExPathfindingNavmeshSettings::GetMainInputLabel() const { return PCGExPathfinding::SourceSeedsLabel; }
FName UPCGExPathfindingNavmeshSettings::GetMainOutputLabel() const { return PCGExGraph::OutputPathsLabel; }

PCGEX_INITIALIZE_ELEMENT(PathfindingNavmesh)

void UPCGExPathfindingNavmeshSettings::PostInitProperties()
{
	Super::PostInitProperties();
	PCGEX_OPERATION_DEFAULT(GoalPicker, UPCGExGoalPickerRandom)
	PCGEX_OPERATION_DEFAULT(Blending, UPCGExSubPointsBlendInterpolate)
}

FPCGExPathfindingNavmeshContext::~FPCGExPathfindingNavmeshContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(GoalsPoints)
	PCGEX_DELETE(OutputPaths)

	PCGEX_DELETE_TARRAY(PathBuffer)
}

bool FPCGExPathfindingNavmeshElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingNavmesh)

	PCGEX_OPERATION_BIND(GoalPicker, UPCGExGoalPickerRandom)
	PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendInterpolate)

	if (TArray<FPCGTaggedData> Goals = Context->InputData.GetInputsByPin(PCGExPathfinding::SourceGoalsLabel);
		Goals.Num() > 0)
	{
		const FPCGTaggedData& GoalsSource = Goals[0];
		Context->GoalsPoints = PCGExData::PCGExPointIO::GetPointIO(Context, GoalsSource);
	}

	if (!Context->GoalsPoints || Context->GoalsPoints->GetNum() == 0)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing Input Goals."));
		return false;
	}

	Context->OutputPaths = new PCGExData::FPointIOCollection();

	PCGEX_FWD(bAddSeedToPath)
	PCGEX_FWD(bAddGoalToPath)

	PCGEX_FWD(NavAgentProperties)
	PCGEX_FWD(bRequireNavigableEndLocation)
	PCGEX_FWD(PathfindingMode)

	Context->FuseDistance = Settings->FuseDistance * Settings->FuseDistance;

	Context->GoalsPoints->CreateInKeys();

	return true;
}

bool FPCGExPathfindingNavmeshElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathfindingNavmeshElement::Execute);

	PCGEX_CONTEXT(PathfindingNavmesh)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->AdvancePointsIO();
		Context->GoalPicker->PrepareForData(*Context->CurrentIO, *Context->GoalsPoints);
		Context->SetState(PCGExMT::State_ProcessingPoints);
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		auto NavClusterTask = [&](const int32 SeedIndex, const int32 GoalIndex)
		{
			const int32 PathIndex = Context->PathBuffer.Add(
				new PCGExPathfinding::FPathQuery(
					SeedIndex, Context->CurrentIO->GetInPoint(SeedIndex).Transform.GetLocation(),
					GoalIndex, Context->GoalsPoints->GetInPoint(GoalIndex).Transform.GetLocation()));

			Context->GetAsyncManager()->Start<FSampleNavmeshTask>(PathIndex, Context->CurrentIO, Context->PathBuffer[PathIndex]);
		};

		PCGExPathfinding::ProcessGoals(Context->CurrentIO, Context->GoalPicker, NavClusterTask);
		Context->SetAsyncState(PCGExPathfinding::State_Pathfinding);
	}

	if (Context->IsState(PCGExPathfinding::State_Pathfinding))
	{
		PCGEX_WAIT_ASYNC

		Context->OutputPaths->OutputTo(Context);
		Context->Done();
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}

bool FSampleNavmeshTask::ExecuteTask()
{
	FPCGExPathfindingNavmeshContext* Context = static_cast<FPCGExPathfindingNavmeshContext*>(Manager->Context);


	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Context->World);

	if (!NavSys || !NavSys->GetDefaultNavDataInstance()) { return false; }

	const FPCGPoint* Seed = Context->CurrentIO->TryGetInPoint(Query->SeedIndex);
	const FPCGPoint* Goal = Context->GoalsPoints->TryGetInPoint(Query->GoalIndex);

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

	if (PathLocations.Num() <= 2) { return false; } //


	const int32 NumPositions = PathLocations.Num();
	const int32 LastPosition = NumPositions - 1;

	PCGExData::FPointIO& PathPoints = Context->OutputPaths->Emplace_GetRef(*PointIO, PCGExData::EInit::NewOutput);
	UPCGPointData* OutData = PathPoints.GetOut();
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
		Context->Blending->CreateBlender(PathPoints, *Context->GoalsPoints);

	TArrayView<FPCGPoint> View(MutablePoints);
	Context->Blending->BlendSubPoints(View, Metrics, TempBlender);

	TempBlender->Write();

	PCGEX_DELETE(TempBlender)

	if (!Context->bAddSeedToPath) { MutablePoints.RemoveAt(0); }
	if (!Context->bAddGoalToPath) { MutablePoints.Pop(); }

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
