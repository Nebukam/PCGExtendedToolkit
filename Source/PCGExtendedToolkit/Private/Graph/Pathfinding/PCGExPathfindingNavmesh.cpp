﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingNavmesh.h"


#include "PCGExPointsProcessor.h"
#include "PCGParamData.h"
#include "Data/PCGExPointIO.h"
#include "Graph/PCGExGraph.h"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExPathfindingNavmeshElement"
#define PCGEX_NAMESPACE PathfindingNavmesh

TArray<FPCGPinProperties> UPCGExPathfindingNavmeshSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINT(PCGExGraph::SourceSeedsLabel, "Seeds points for pathfinding.", Required)
	PCGEX_PIN_POINT(PCGExGraph::SourceGoalsLabel, "Goals points for pathfinding.", Required)
	PCGEX_PIN_OPERATION_OVERRIDES(PCGExPathfinding::SourceOverridesGoalPicker)
	PCGEX_PIN_OPERATION_OVERRIDES(PCGExDataBlending::SourceOverridesBlendingOps)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPathfindingNavmeshSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExPaths::OutputPathsLabel, "Paths output.", Required)
	return PinProperties;
}

#if WITH_EDITOR

void UPCGExPathfindingNavmeshSettings::PostInitProperties()
{
	if (!HasAnyFlags(RF_ClassDefaultObject) && IsInGameThread())
	{
		if (!GoalPicker) { GoalPicker = NewObject<UPCGExGoalPicker>(this, TEXT("GoalPicker")); }
		if (!Blending) { Blending = NewObject<UPCGExSubPointsBlendInterpolate>(this, TEXT("Blending")); }
	}
	Super::PostInitProperties();
}

void UPCGExPathfindingNavmeshSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (GoalPicker) { GoalPicker->UpdateUserFacingInfos(); }
	if (Blending) { Blending->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

FName UPCGExPathfindingNavmeshSettings::GetMainInputPin() const { return PCGExGraph::SourceSeedsLabel; }

PCGEX_INITIALIZE_ELEMENT(PathfindingNavmesh)


bool FPCGExPathfindingNavmeshElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingNavmesh)

	PCGEX_OPERATION_BIND(GoalPicker, UPCGExGoalPicker, PCGExPathfinding::SourceOverridesGoalPicker)
	PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendInstancedFactory, PCGExDataBlending::SourceOverridesBlendingOps)

	Context->SeedsDataFacade = PCGExData::TryGetSingleFacade(Context, PCGExGraph::SourceSeedsLabel, false, true);
	Context->GoalsDataFacade = PCGExData::TryGetSingleFacade(Context, PCGExGraph::SourceGoalsLabel, false, true);

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
	Context->OutputPaths->OutputPin = PCGExPaths::OutputPathsLabel;

	// Prepare path queries

	if (!Context->GoalPicker->PrepareForData(Context, Context->SeedsDataFacade, Context->GoalsDataFacade)) { return false; }

	PCGExPathfinding::ProcessGoals(
		Context->SeedsDataFacade, Context->GoalPicker,
		[&](const int32 SeedIndex, const int32 GoalIndex)
		{
			Context->PathQueries.Emplace(
				SeedIndex, Context->SeedsDataFacade->Source->GetInPoint(SeedIndex).GetLocation(),
				GoalIndex, Context->GoalsDataFacade->Source->GetInPoint(GoalIndex).GetLocation());
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
				SeedIndex, Context->SeedsDataFacade->Source->GetInPoint(SeedIndex).GetLocation(),
				GoalIndex, Context->GoalsDataFacade->Source->GetInPoint(GoalIndex).GetLocation());

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

FSampleNavmeshTask::FSampleNavmeshTask(const int32 InTaskIndex, const TSharedPtr<PCGExData::FPointIO>& InPointIO, const TArray<PCGExPathfinding::FSeedGoalPair>* InQueries)
	: FPCGExPathfindingTask(InTaskIndex, InPointIO, InQueries)
{
}

void FSampleNavmeshTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
{
	FPCGExPathfindingNavmeshContext* Context = AsyncManager->GetContext<FPCGExPathfindingNavmeshContext>();
	PCGEX_SETTINGS(PathfindingNavmesh)

	PCGExNavmesh::FNavmeshQuery Query((*Queries)[TaskIndex]);
	Query.FindPath(Context);

	if (!Query.IsValid()) { return; }

	const UPCGBasePointData* SeedsData = Context->SeedsDataFacade->GetIn();
	const UPCGBasePointData* GoalsData = Context->GoalsDataFacade->GetIn();

	PCGExData::FConstPoint Seed(SeedsData, Query.SeedGoalPair.Seed);
	PCGExData::FConstPoint Goal(GoalsData, Query.SeedGoalPair.Goal);

	const int32 NumPositions = Query.Positions.Num() + Settings->bAddSeedToPath + Settings->bAddGoalToPath;

	if (NumPositions <= 2) { return; }

	TSharedPtr<PCGExData::FPointIO> PathIO = Context->OutputPaths->Emplace_GetRef(PointIO, PCGExData::EIOInit::New);
	PCGEX_MAKE_SHARED(PathDataFacade, PCGExData::FFacade, PathIO.ToSharedRef())

	UPCGBasePointData* OutData = PathIO->GetOut();
	PCGEx::SetNumPointsAllocated(OutData, NumPositions);

	int32 WriteIndex = 0;
	TPCGValueRange<FTransform> OutTransforms = OutData->GetTransformValueRange(false);
	Query.CopyPositions(OutTransforms, WriteIndex, Settings->bAddGoalToPath, Settings->bAddGoalToPath);

	TSharedPtr<FPCGExSubPointsBlendOperation> SubBlending = Context->Blending->CreateOperation();
	if (!SubBlending->PrepareForData(Context, PathDataFacade, Context->GoalsDataFacade, PCGExData::EIOSide::In)) { return; }

	// TODO : Need to ensure the metadata blender can pick source A as target IN, if relevant
	// cause we might try to blend from a point that's technically in the same data but also not

	PCGExData::FScope SubScope = PathIO->GetOutScope(0 + Settings->bAddSeedToPath, NumPositions - (Settings->bAddSeedToPath + Settings->bAddGoalToPath));
	SubBlending->BlendSubPoints(Seed, Goal, SubScope, Query.SeedGoalMetrics);

	Context->SeedAttributesToPathTags.Tag(Seed, PathIO);
	Context->GoalAttributesToPathTags.Tag(Goal, PathIO);

	Context->SeedForwardHandler->Forward(Seed.Index, PathDataFacade);
	Context->GoalForwardHandler->Forward(Goal.Index, PathDataFacade);

	PathDataFacade->WriteFastest(AsyncManager);
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
