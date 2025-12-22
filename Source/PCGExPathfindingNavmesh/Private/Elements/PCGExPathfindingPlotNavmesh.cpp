// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExPathfindingPlotNavmesh.h"

#include "Clusters/PCGExClusterCommon.h"
#include "PCGParamData.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Paths/PCGExPathsCommon.h"
#include "SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExPathfindingPlotNavmeshElement"
#define PCGEX_NAMESPACE PathfindingPlotNavmesh

TArray<FPCGPinProperties> UPCGExPathfindingPlotNavmeshSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_OPERATION_OVERRIDES(PCGExBlending::Labels::SourceOverridesBlendingOps)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPathfindingPlotNavmeshSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExPaths::Labels::OutputPathsLabel, "Paths output.", Required)
	return PinProperties;
}

#if WITH_EDITOR
void UPCGExPathfindingPlotNavmeshSettings::PostInitProperties()
{
	if (!HasAnyFlags(RF_ClassDefaultObject) && IsInGameThread())
	{
		if (!Blending) { Blending = NewObject<UPCGExSubPointsBlendInterpolate>(this, TEXT("Blending")); }
	}
	Super::PostInitProperties();
}

void UPCGExPathfindingPlotNavmeshSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Blending) { Blending->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

FName UPCGExPathfindingPlotNavmeshSettings::GetMainInputPin() const { return PCGExClusters::Labels::SourcePlotsLabel; }

PCGEX_INITIALIZE_ELEMENT(PathfindingPlotNavmesh)


bool FPCGExPathfindingPlotNavmeshElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingPlotNavmesh)

	PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendInstancedFactory, PCGExBlending::Labels::SourceOverridesBlendingOps)

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

bool FPCGExPathfindingPlotNavmeshElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathfindingPlotNavmeshElement::Execute);

	PCGEX_CONTEXT(PathfindingPlotNavmesh)
	PCGEX_EXECUTION_CHECK

	PCGEX_ON_INITIAL_EXECUTION
	{
		const TSharedPtr<PCGExMT::FTaskManager> TaskManager = Context->GetTaskManager();
		while (Context->AdvancePointsIO(false))
		{
			if (Context->CurrentIO->GetNum() < 2) { continue; }
			PCGEX_LAUNCH(FPCGExPlotNavmeshTask, Context->CurrentIO)
		}
		Context->SetState(PCGExCommon::States::State_ProcessingPoints);
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGExCommon::States::State_ProcessingPoints)
	{
		Context->OutputPaths->StageOutputs();
		Context->Done();
	}

	return Context->TryComplete();
}

void FPCGExPlotNavmeshTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager)
{
	FPCGExPathfindingPlotNavmeshContext* Context = TaskManager->GetContext<FPCGExPathfindingPlotNavmeshContext>();
	PCGEX_SETTINGS(PathfindingPlotNavmesh)

	const int32 NumPlots = PointIO->GetNum();

	TArray<PCGExNavmesh::FNavmeshQuery> PlotQueries;
	PlotQueries.Reserve(NumPlots);

	auto PlotQuery = [&](const int32 SeedIndex, const int32 GoalIndex)-> PCGExNavmesh::FNavmeshQuery&
	{
		PCGExNavmesh::FNavmeshQuery& Query = PlotQueries.Emplace_GetRef(PCGExPathfinding::FSeedGoalPair(PointIO->GetInPoint(SeedIndex), PointIO->GetInPoint(GoalIndex)));
		Query.FindPath(Context);
		return Query;
	};


	PCGExPointArrayDataHelpers::FReadWriteScope PlotScope(NumPlots + 2, false);

	// First, compute the final number of points
	int32 NumPoints = 0;

	for (int i = 0; i < NumPlots - 1; i++)
	{
		PCGExNavmesh::FNavmeshQuery& Query = PlotQuery(i, i + 1);

		if (i == 0 && Settings->bAddSeedToPath)
		{
			// First query and we want seed in the mix
			PlotScope.Add(Query.SeedGoalPair.Seed, NumPoints++);
		}

		NumPoints += Query.Positions.Num();

		if (Settings->bAddPlotPointsToPath || (i == NumPlots - 2 && !Settings->bClosedLoop && Settings->bAddGoalToPath))
		{
			// Either last query & we want goals,
			// or we want plot points, in which case we insert goals (since any non-last goal is the next query' seed)
			PlotScope.Add(Query.SeedGoalPair.Goal, NumPoints++);
		}
	}

	if (Settings->bClosedLoop)
	{
		PCGExNavmesh::FNavmeshQuery& Query = PlotQuery(NumPlots - 1, 0);
		NumPoints += Query.Positions.Num();
		// No extras, it's a wrapping path
	}

	if (NumPoints <= 2) { return; }

	// Initialize data
	TSharedPtr<PCGExData::FPointIO> PathIO = Context->OutputPaths->Emplace_GetRef(PointIO, PCGExData::EIOInit::New);
	PCGEX_MAKE_SHARED(PathDataFacade, PCGExData::FFacade, PathIO.ToSharedRef())

	UPCGBasePointData* OutPathData = PathIO->GetOut();
	PCGExPointArrayDataHelpers::SetNumPointsAllocated(OutPathData, NumPoints);

	// Copy seed/goal properties
	PlotScope.CopyPoints(PointIO->GetIn(), PathIO->GetOut());

	TSharedPtr<FPCGExSubPointsBlendOperation> SubBlending = Context->Blending->CreateOperation();
	if (!SubBlending->PrepareForData(Context, PathDataFacade)) { return; }

	TPCGValueRange<FTransform> OutTransforms = OutPathData->GetTransformValueRange(false);

	const int32 LastPlotIndex = PlotQueries.Num() - 1;
	int32 WriteIndex = Settings->bAddSeedToPath; // Start at 1 if we added the seed
	for (int i = 0; i < PlotQueries.Num(); i++)
	{
		PCGExNavmesh::FNavmeshQuery& Query = PlotQueries[i];

		const int32 StartIndex = WriteIndex;
		Query.CopyPositions(OutTransforms, WriteIndex, false, false);

		PCGExData::FScope SubScope(PathIO->GetOut(), StartIndex, Query.Positions.Num());
		if (SubScope.IsValid())
		{
			SubBlending->BlendSubPoints(PointIO->GetInPoint(Query.SeedGoalPair.Seed), PointIO->GetInPoint(Query.SeedGoalPair.Goal), SubScope, Query.SeedGoalMetrics);
		}

		// Pad index if we inserted plot points
		if (i != LastPlotIndex && Settings->bAddPlotPointsToPath) { WriteIndex++; }
	}

	PathDataFacade->WriteFastest(TaskManager);
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
