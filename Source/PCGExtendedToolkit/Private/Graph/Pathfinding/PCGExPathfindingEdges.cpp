// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingEdges.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Graph/Pathfinding/Search/PCGExSearchOperation.h"
#include "Paths/PCGExPaths.h"

#define LOCTEXT_NAMESPACE "PCGExPathfindingEdgesElement"
#define PCGEX_NAMESPACE PathfindingEdges

#if WITH_EDITOR
void UPCGExPathfindingEdgesSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (GoalPicker) { GoalPicker->UpdateUserFacingInfos(); }
	if (SearchAlgorithm) { SearchAlgorithm->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
void FPCGExPathfindingEdgesContext::BuildPath(const TSharedPtr<PCGExPathfinding::FPathQuery>& Query)
{
	PCGEX_SETTINGS_LOCAL(PathfindingEdges)

	TArray<int32> PathIndices;
	int32 ExtraIndices = 0;
	PathIndices.Reserve(Query->PathNodes.Num() + 2);

	TSharedPtr<PCGExData::FPointIO> ReferenceIO = nullptr;

	if (Settings->PathComposition == EPCGExPathComposition::Vtx)
	{
		ReferenceIO = Query->Cluster->VtxIO.Pin();
		Query->AppendNodePoints(PathIndices);
	}
	else if (Settings->PathComposition == EPCGExPathComposition::Edges)
	{
		ReferenceIO = Query->Cluster->EdgesIO.Pin();
		Query->AppendEdgePoints(PathIndices);
	}
	else if (Settings->PathComposition == EPCGExPathComposition::VtxAndEdges)
	{
		// TODO : Implement
	}

	if (Settings->PathComposition == EPCGExPathComposition::Vtx)
	{
		if (PathIndices.Num() < 2) { return; }
	}
	else if (Settings->PathComposition == EPCGExPathComposition::Edges)
	{
		if (PathIndices.Num() < 1) { return; }
	}
	else if (Settings->PathComposition == EPCGExPathComposition::VtxAndEdges)
	{
		// TODO : Implement
	}

	if (Settings->bAddSeedToPath) { ExtraIndices++; }
	if (Settings->bAddGoalToPath) { ExtraIndices++; }

	if (!Settings->PathOutputDetails.Validate(PathIndices.Num() + ExtraIndices)) { return; }

	const TSharedPtr<PCGExData::FPointIO> PathIO = OutputPaths->Emplace_GetRef<UPCGPointArrayData>(ReferenceIO, PCGExData::EIOInit::New);
	if (!PathIO) { return; }

	EPCGPointNativeProperties AllocateProperties = ReferenceIO->GetAllocations();
	EnumAddFlags(AllocateProperties, SeedsDataFacade->GetAllocations());
	EnumAddFlags(AllocateProperties, GoalsDataFacade->GetAllocations());

	PathIO->IOIndex = Query->QueryIndex;
	UPCGBasePointData* PathPoints = PathIO->GetOut();
	PCGEx::SetNumPointsAllocated(PathPoints, PathIndices.Num() + ExtraIndices, AllocateProperties);

	PathIO->InheritPoints(PathIndices, Settings->bAddSeedToPath ? 1 : 0);

	if (Settings->bAddSeedToPath)
	{
		Query->Seed.Point.Data->CopyPropertiesTo(PathPoints, Query->Seed.Point.Index, 0, 1, AllocateProperties);
	}

	if (Settings->bAddGoalToPath)
	{
		Query->Goal.Point.Data->CopyPropertiesTo(PathPoints, Query->Goal.Point.Index, PathPoints->GetNumPoints() - 1, 1, AllocateProperties);
	}

	PCGExGraph::CleanupClusterData(PathIO);

	PCGEX_MAKE_SHARED(PathDataFacade, PCGExData::FFacade, PathIO.ToSharedRef())

	SeedAttributesToPathTags.Tag(Query->Seed, PathIO);
	GoalAttributesToPathTags.Tag(Query->Goal, PathIO);

	SeedForwardHandler->Forward(Query->Seed.Point.Index, PathDataFacade);
	GoalForwardHandler->Forward(Query->Goal.Point.Index, PathDataFacade);

	PathDataFacade->WriteFastest(GetAsyncManager());
}

PCGEX_INITIALIZE_ELEMENT(PathfindingEdges)
PCGEX_ELEMENT_BATCH_EDGE_IMPL(PathfindingEdges)

TArray<FPCGPinProperties> UPCGExPathfindingEdgesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGExGraph::SourceSeedsLabel, "Seeds points for pathfinding.", Required, {})
	PCGEX_PIN_POINT(PCGExGraph::SourceGoalsLabel, "Goals points for pathfinding.", Required, {})
	PCGEX_PIN_FACTORIES(PCGExGraph::SourceHeuristicsLabel, "Heuristics.", Normal, {})
	PCGEX_PIN_OPERATION_OVERRIDES(PCGExPathfinding::SourceOverridesGoalPicker)
	PCGEX_PIN_OPERATION_OVERRIDES(PCGExPathfinding::SourceOverridesSearch)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPathfindingEdgesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExPaths::OutputPathsLabel, "Paths output.", Required, {})
	return PinProperties;
}

PCGExData::EIOInit UPCGExPathfindingEdgesSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::NoInit; }
PCGExData::EIOInit UPCGExPathfindingEdgesSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::NoInit; }

bool FPCGExPathfindingEdgesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingEdges)

	PCGEX_OPERATION_BIND(GoalPicker, UPCGExGoalPicker, PCGExPathfinding::SourceOverridesGoalPicker)
	PCGEX_OPERATION_BIND(SearchAlgorithm, UPCGExSearchInstancedFactory, PCGExPathfinding::SourceOverridesSearch)

	Context->SeedsDataFacade = PCGExData::TryGetSingleFacade(Context, PCGExGraph::SourceSeedsLabel, false, true);
	if (!Context->SeedsDataFacade) { return false; }

	Context->GoalsDataFacade = PCGExData::TryGetSingleFacade(Context, PCGExGraph::SourceGoalsLabel, false, true);
	if (!Context->GoalsDataFacade) { return false; }

	PCGEX_FWD(SeedAttributesToPathTags)
	PCGEX_FWD(GoalAttributesToPathTags)

	if (!Context->SeedAttributesToPathTags.Init(Context, Context->SeedsDataFacade)) { return false; }
	if (!Context->GoalAttributesToPathTags.Init(Context, Context->GoalsDataFacade)) { return false; }

	Context->SeedForwardHandler = Settings->SeedForwarding.GetHandler(Context->SeedsDataFacade);
	Context->GoalForwardHandler = Settings->GoalForwarding.GetHandler(Context->GoalsDataFacade);

	Context->OutputPaths = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->OutputPaths->OutputPin = PCGExPaths::OutputPathsLabel;

	// Prepare path seed/goal pairs

	if (!Context->GoalPicker->PrepareForData(Context, Context->SeedsDataFacade, Context->GoalsDataFacade)) { return false; }

	PCGExPathfinding::ProcessGoals(
		Context->SeedsDataFacade, Context->GoalPicker,
		[&](const int32 SeedIndex, const int32 GoalIndex)
		{
			Context->SeedGoalPairs.Add(PCGEx::H64(SeedIndex, GoalIndex));
		});

	if (Context->SeedGoalPairs.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Could not generate any seed/goal pairs."));
		return false;
	}

	return true;
}

bool FPCGExPathfindingEdgesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathfindingEdgesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingEdges)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
			{
				NewBatch->SetWantsHeuristics(true);
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->OutputPaths->StageOutputs();

	return Context->TryComplete();
}


namespace PCGExPathfindingEdges
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathfindingEdges::Process);

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		if (Settings->bUseOctreeSearch)
		{
			if (Settings->SeedPicking.PickingMethod == EPCGExClusterClosestSearchMode::Vtx ||
				Settings->GoalPicking.PickingMethod == EPCGExClusterClosestSearchMode::Vtx)
			{
				Cluster->RebuildOctree(EPCGExClusterClosestSearchMode::Vtx);
			}

			if (Settings->SeedPicking.PickingMethod == EPCGExClusterClosestSearchMode::Edge ||
				Settings->GoalPicking.PickingMethod == EPCGExClusterClosestSearchMode::Edge)
			{
				Cluster->RebuildOctree(EPCGExClusterClosestSearchMode::Edge);
			}
		}

		SearchOperation = Context->SearchAlgorithm->CreateOperation(); // Create a local copy
		SearchOperation->PrepareForCluster(Cluster.Get());

		PCGEx::InitArray(Queries, Context->SeedGoalPairs.Num());
		for (int i = 0; i < Queries.Num(); i++)
		{
			TSharedPtr<PCGExPathfinding::FPathQuery> Query = MakeShared<PCGExPathfinding::FPathQuery>(
				Cluster.ToSharedRef(),
				Context->SeedsDataFacade->Source->GetInPoint(PCGEx::H64A(Context->SeedGoalPairs[i])),
				Context->GoalsDataFacade->Source->GetInPoint(PCGEx::H64B(Context->SeedGoalPairs[i])),
				i);

			Queries[i] = Query;
		}

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, ResolveQueriesTask)
		ResolveQueriesTask->OnIterationCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const int32 Index, const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS

				TSharedPtr<PCGExPathfinding::FPathQuery> Query = This->Queries[Index];
				Query->ResolvePicks(This->Settings->SeedPicking, This->Settings->GoalPicking);

				if (!Query->HasValidEndpoints()) { return; }

				Query->FindPath(This->SearchOperation, This->HeuristicsHandler, nullptr);

				if (!Query->IsQuerySuccessful()) { return; }

				This->Context->BuildPath(Query);
				Query->Cleanup();
			};

		ResolveQueriesTask->StartIterations(Queries.Num(), 1, HeuristicsHandler->HasGlobalFeedback());
		return true;
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
