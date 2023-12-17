// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingNavmesh.h"

#include "NavigationSystem.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "Graph/Pathfinding/PCGExPathfinding.h"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExPathfindingNavmeshElement"
#define PCGEX_NAMESPACE PathfindingNavmesh

UPCGExPathfindingNavmeshSettings::UPCGExPathfindingNavmeshSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PCGEX_DEFAULT_OPERATION(GoalPicker, UPCGExGoalPickerRandom)
	PCGEX_DEFAULT_OPERATION(Blending, UPCGExSubPointsBlendInterpolate)
}

TArray<FPCGPinProperties> UPCGExPathfindingNavmeshSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	FPCGPinProperties& PinPropertySeeds = PinProperties.Emplace_GetRef(PCGExPathfinding::SourceSeedsLabel, EPCGDataType::Point, false, false);

#if WITH_EDITOR
	PinPropertySeeds.Tooltip = FTEXT("Seeds points for pathfinding.");
#endif // WITH_EDITOR

	FPCGPinProperties& PinPropertyGoals = PinProperties.Emplace_GetRef(PCGExPathfinding::SourceGoalsLabel, EPCGDataType::Point, false, false);

#if WITH_EDITOR
	PinPropertyGoals.Tooltip = FTEXT("Goals points for pathfinding.");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPathfindingNavmeshSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPathsOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputPathsLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPathsOutput.Tooltip = FTEXT("Paths output.");
#endif // WITH_EDITOR

	return PinProperties;
}

void UPCGExPathfindingNavmeshSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (GoalPicker) { GoalPicker->UpdateUserFacingInfos(); }
	if (Blending) { Blending->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

PCGExData::EInit UPCGExPathfindingNavmeshSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }
int32 UPCGExPathfindingNavmeshSettings::GetPreferredChunkSize() const { return 32; }

FName UPCGExPathfindingNavmeshSettings::GetMainInputLabel() const { return PCGExPathfinding::SourceSeedsLabel; }
FName UPCGExPathfindingNavmeshSettings::GetMainOutputLabel() const { return PCGExGraph::OutputPathsLabel; }

FPCGExPathfindingNavmeshContext::~FPCGExPathfindingNavmeshContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(GoalsPoints)
	PCGEX_DELETE(OutputPaths)

	PCGEX_DELETE_TARRAY(PathBuffer)
}

FPCGElementPtr UPCGExPathfindingNavmeshSettings::CreateElement() const { return MakeShared<FPCGExPathfindingNavmeshElement>(); }

PCGEX_INITIALIZE_CONTEXT(PathfindingNavmesh)

bool FPCGExPathfindingNavmeshElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingNavmesh)

	PCGEX_BIND_OPERATION(GoalPicker, UPCGExGoalPickerRandom)
	PCGEX_BIND_OPERATION(Blending, UPCGExSubPointsBlendInterpolate)

	if (TArray<FPCGTaggedData> Goals = Context->InputData.GetInputsByPin(PCGExPathfinding::SourceGoalsLabel);
		Goals.Num() > 0)
	{
		const FPCGTaggedData& GoalsSource = Goals[0];
		Context->GoalsPoints = PCGExData::PCGExPointIO::GetPointIO(Context, GoalsSource);
	}

	if (!Settings->NavData)
	{
		if (const UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Context->World))
		{
			ANavigationData* NavData = NavSys->GetDefaultNavDataInstance();
			Context->NavData = NavData;
		}
	}

	if (!Context->GoalsPoints || Context->GoalsPoints->GetNum() == 0)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing Input Goals."));
		return false;
	}

	if (!Context->NavData)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing Nav Data"));
		return false;
	}

	Context->OutputPaths = new PCGExData::FPointIOGroup();

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
		auto Initialize = []()
		{
		};

		auto NavMeshTask = [&](int32 SeedIndex, int32 GoalIndex)
		{
			Context->BufferLock.WriteLock();
			const int32 PathIndex = Context->PathBuffer.Add(
				new PCGExPathfinding::FPathQuery(
					SeedIndex, Context->CurrentIO->GetInPoint(SeedIndex).Transform.GetLocation(),
					GoalIndex, Context->GoalsPoints->GetInPoint(GoalIndex).Transform.GetLocation()));
			Context->BufferLock.WriteUnlock();
			Context->GetAsyncManager()->Start<FSampleNavmeshTask>(PathIndex, Context->CurrentIO, Context->PathBuffer[PathIndex]);
		};

		if (PCGExPathfinding::ProcessGoals(Initialize, Context, Context->CurrentIO, Context->GoalPicker, NavMeshTask))
		{
			Context->SetAsyncState(PCGExPathfinding::State_Pathfinding);
		}
	}

	if (Context->IsState(PCGExPathfinding::State_Pathfinding))
	{
		if (Context->IsAsyncWorkComplete())
		{
			Context->OutputPaths->OutputTo(Context, true);
			Context->Done();
		}
	}

	return Context->IsDone();
}

bool FSampleNavmeshTask::ExecuteTask()
{
	FPCGExPathfindingNavmeshContext* Context = static_cast<FPCGExPathfindingNavmeshContext*>(Manager->Context);
	PCGEX_ASYNC_CHECKPOINT

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Context->World);

	if (!NavSys) { return false; }

	const FPCGPoint* Seed = Context->CurrentIO->TryGetInPoint(Query->SeedIndex);
	const FPCGPoint* Goal = Context->GoalsPoints->TryGetInPoint(Query->GoalIndex);

	if (!Seed || !Goal) { return false; }

	FPathFindingQuery PathFindingQuery = FPathFindingQuery(
		Context->World, *Context->NavData,
		Query->SeedPosition, Query->GoalPosition, nullptr, nullptr,
		TNumericLimits<FVector::FReal>::Max(),
		Context->bRequireNavigableEndLocation);

	PathFindingQuery.NavAgentProperties = Context->NavAgentProperties;

	const FPathFindingResult Result = NavSys->FindPathSync(
		Context->NavAgentProperties, PathFindingQuery,
		Context->PathfindingMode == EPCGExPathfindingNavmeshMode::Regular ? EPathFindingMode::Type::Regular : EPathFindingMode::Type::Hierarchical);

	if (Result.Result != ENavigationQueryResult::Type::Success) { return false; } ///
	PCGEX_ASYNC_CHECKPOINT

	const TArray<FNavPathPoint>& Points = Result.Path->GetPathPoints();

	TArray<FVector> PathLocations;
	PathLocations.Reserve(Points.Num());

	PathLocations.Add(Query->SeedPosition);
	for (FNavPathPoint PathPoint : Points) { PathLocations.Add(PathPoint.Location); }
	PathLocations.Add(Query->GoalPosition);

	PCGExMath::FPathMetrics Metrics = PCGExMath::FPathMetrics(Query->SeedPosition);
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
	PCGEX_ASYNC_CHECKPOINT

	const int32 NumPositions = PathLocations.Num();
	const int32 LastPosition = NumPositions - 1;

	PCGExData::FPointIO& PathPoints = Context->OutputPaths->Emplace_GetRef(*Context->CurrentIO, PCGExData::EInit::NewOutput);
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

	const PCGExDataBlending::FMetadataBlender* TempBlender = Context->Blending->CreateBlender(
		OutData, Context->GoalsPoints->GetIn(),
		PathPoints.CreateOutKeys(), Context->GoalsPoints->GetInKeys());

	TArrayView<FPCGPoint> View(MutablePoints);
	Context->Blending->BlendSubPoints(View, Metrics, TempBlender);

	PCGEX_DELETE(TempBlender)

	if (!Context->bAddSeedToPath) { MutablePoints.RemoveAt(0); }
	if (!Context->bAddGoalToPath) { MutablePoints.Pop(); }

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
