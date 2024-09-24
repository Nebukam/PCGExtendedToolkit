// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingEdges.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "PCGExPathfinding.cpp"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Graph/Pathfinding/Search/PCGExSearchOperation.h"
#include "Algo/Reverse.h"
#include "Graph/Pathfinding/Search/PCGExSearchAStar.h"

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
void FPCGExPathfindingEdgesContext::TryFindPath(
	const UPCGExSearchOperation* SearchOperation,
	const PCGExPathfinding::FPathQuery* Query,
	PCGExHeuristics::THeuristicsHandler* HeuristicsHandler)
{
	// TODO : Vtx OR/AND edge points

	PCGEX_SETTINGS_LOCAL(PathfindingEdges)

	const FPCGPoint& Seed = SeedsDataFacade->Source->GetInPoint(Query->SeedIndex);
	const FPCGPoint& Goal = GoalsDataFacade->Source->GetInPoint(Query->GoalIndex);

	PCGExCluster::FCluster* Cluster = SearchOperation->Cluster;
	const TArray<int32>& VtxPointIndices = Cluster->GetVtxPointIndices();

	TArray<int32> Path;

	//Note: Can silently fail
	if (!SearchOperation->FindPath(
		Query->SeedPosition, &Settings->SeedPicking,
		Query->GoalPosition, &Settings->GoalPicking, HeuristicsHandler, Path))
	{
		// Failed
		return;
	}

	if (Path.Num() < 2 && !Settings->bAddSeedToPath && !Settings->bAddGoalToPath)
	{
		// Omit
		return;
	}

	TSharedPtr<PCGExData::FPointIO> PathIO = OutputPaths->Emplace_GetRef<UPCGPointData>(Cluster->VtxIO->GetIn(), PCGExData::EInit::NewOutput);
	UPCGPointData* OutData = PathIO->GetOut();
	PCGExData::FFacade* PathDataFacade = new PCGExData::FFacade(PathIO);

	PCGExGraph::CleanupClusterTags(PathIO.Get(), true);
	PCGExGraph::CleanupVtxData(PathIO.Get());

	TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();
	const TArray<FPCGPoint>& InPoints = Cluster->VtxIO->GetIn()->GetPoints();

	MutablePoints.Reserve(Path.Num() + 2);

	if (Settings->bAddSeedToPath) { MutablePoints.Add_GetRef(Seed).MetadataEntry = PCGInvalidEntryKey; }
	for (const int32 VtxIndex : Path) { MutablePoints.Add(InPoints[VtxPointIndices[VtxIndex]]); }
	if (Settings->bAddGoalToPath) { MutablePoints.Add_GetRef(Goal).MetadataEntry = PCGInvalidEntryKey; }

	SeedAttributesToPathTags.Tag(Query->SeedIndex, PathIO.Get());
	GoalAttributesToPathTags.Tag(Query->GoalIndex, PathIO.Get());

	SeedForwardHandler->Forward(Query->SeedIndex, PathDataFacade);
	GoalForwardHandler->Forward(Query->GoalIndex, PathDataFacade);

	PathDataFacade->Write(GetAsyncManager());
	PCGEX_DELETE(PathDataFacade)
}

PCGEX_INITIALIZE_ELEMENT(PathfindingEdges)

TArray<FPCGPinProperties> UPCGExPathfindingEdgesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGExGraph::SourceSeedsLabel, "Seeds points for pathfinding.", Required, {})
	PCGEX_PIN_POINT(PCGExGraph::SourceGoalsLabel, "Goals points for pathfinding.", Required, {})
	PCGEX_PIN_PARAMS(PCGExGraph::SourceHeuristicsLabel, "Heuristics.", Normal, {})
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPathfindingEdgesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(PCGExGraph::OutputPathsLabel, "Paths output.", Required, {})
	return PinProperties;
}

PCGExData::EInit UPCGExPathfindingEdgesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }
PCGExData::EInit UPCGExPathfindingEdgesSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FPCGExPathfindingEdgesContext::~FPCGExPathfindingEdgesContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE_TARRAY(PathQueries)

	PCGEX_DELETE(OutputPaths)

	SeedAttributesToPathTags.Cleanup();
	GoalAttributesToPathTags.Cleanup();

	PCGEX_DELETE(SeedForwardHandler)
	PCGEX_DELETE(GoalForwardHandler)
}


bool FPCGExPathfindingEdgesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingEdges)

	PCGEX_OPERATION_BIND(GoalPicker, UPCGExGoalPicker)
	PCGEX_OPERATION_BIND(SearchAlgorithm, UPCGExSearchOperation)

	const TSharedPtr<PCGExData::FPointIO> SeedsPoints = PCGExData::TryGetSingleInput(Context, PCGExGraph::SourceSeedsLabel, true);
	if (!SeedsPoints) { return false; }

	Context->SeedsDataFacade = MakeUnique<PCGExData::FFacade>(SeedsPoints);

	const TSharedPtr<PCGExData::FPointIO> GoalsPoints = PCGExData::TryGetSingleInput(Context, PCGExGraph::SourceGoalsLabel, true);
	if (!GoalsPoints) { return false; }

	Context->GoalsDataFacade = MakeUnique<PCGExData::FFacade>(GoalsPoints);

	PCGEX_FWD(SeedAttributesToPathTags)
	PCGEX_FWD(GoalAttributesToPathTags)

	if (!Context->SeedAttributesToPathTags.Init(Context, Context->SeedsDataFacade.Get())) { return false; }
	if (!Context->GoalAttributesToPathTags.Init(Context, Context->GoalsDataFacade.Get())) { return false; }

	Context->SeedForwardHandler = Settings->SeedForwarding.GetHandler(Context->SeedsDataFacade.Get());
	Context->GoalForwardHandler = Settings->GoalForwarding.GetHandler(Context->GoalsDataFacade.Get());

	Context->OutputPaths = new PCGExData::FPointIOCollection(Context);
	Context->OutputPaths->DefaultOutputLabel = PCGExGraph::OutputPathsLabel;

	// Prepare path queries

	Context->GoalPicker->PrepareForData(Context->SeedsDataFacade.Get(), Context->GoalsDataFacade.Get());
	PCGExPathfinding::ProcessGoals(
		Context->SeedsDataFacade.Get(), Context->GoalPicker,
		[&](const int32 SeedIndex, const int32 GoalIndex)
		{
			Context->PathQueries.Add(
				new PCGExPathfinding::FPathQuery(
					SeedIndex, SeedsPoints->GetInPoint(SeedIndex).Transform.GetLocation(),
					GoalIndex, GoalsPoints->GetInPoint(GoalIndex).Transform.GetLocation()));
		});

	return true;
}

bool FPCGExPathfindingEdgesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathfindingEdgesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingEdges)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatchWithHeuristics<PCGExPathfindingEdge::FProcessor>>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExClusterMT::TBatchWithHeuristics<PCGExPathfindingEdge::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	Context->OutputPaths->OutputToContext();

	return Context->TryComplete();
}


namespace PCGExPathfindingEdge
{
	bool FSampleClusterPathTask::ExecuteTask()
	{
		FPCGExPathfindingEdgesContext* Context = Manager->GetContext<FPCGExPathfindingEdgesContext>();
		PCGEX_SETTINGS(PathfindingEdges)

		Context->TryFindPath(SearchOperation, (*Queries)[TaskIndex], Heuristics);

		if (bInlined && Queries->IsValidIndex(TaskIndex + 1))
		{
			// -> Inline next query
			Manager->Start<FSampleClusterPathTask>(TaskIndex + 1, nullptr, SearchOperation, Queries, Heuristics, true);
		}

		return true;
	}

	FProcessor::~FProcessor()
	{
		PCGEX_DELETE_OPERATION(SearchOperation)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathfindingEdge::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathfindingEdges)

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		if (Settings->bUseOctreeSearch)
		{
			if (Settings->SeedPicking.PickingMethod == EPCGExClusterClosestSearchMode::Node ||
				Settings->GoalPicking.PickingMethod == EPCGExClusterClosestSearchMode::Node)
			{
				Cluster->RebuildOctree(EPCGExClusterClosestSearchMode::Node);
			}

			if (Settings->SeedPicking.PickingMethod == EPCGExClusterClosestSearchMode::Edge ||
				Settings->GoalPicking.PickingMethod == EPCGExClusterClosestSearchMode::Edge)
			{
				Cluster->RebuildOctree(EPCGExClusterClosestSearchMode::Edge);
			}
		}

		SearchOperation = TypedContext->SearchAlgorithm->CopyOperation<UPCGExSearchOperation>(); // Create a local copy
		SearchOperation->PrepareForCluster(Cluster.Get());

		if (IsTrivial())
		{
			// Naturally accounts for global heuristics
			for (const PCGExPathfinding::FPathQuery* Query : TypedContext->PathQueries)
			{
				TypedContext->TryFindPath(SearchOperation, Query, HeuristicsHandler);
			}

			return true;
		}

		if (HeuristicsHandler->HasGlobalFeedback())
		{
			AsyncManagerPtr->Start<FSampleClusterPathTask>(0, VtxIO, SearchOperation, &TypedContext->PathQueries, HeuristicsHandler, true);
		}
		else
		{
			for (int i = 0; i < TypedContext->PathQueries.Num(); ++i)
			{
				AsyncManagerPtr->Start<FSampleClusterPathTask>(i, VtxIO, SearchOperation, &TypedContext->PathQueries, HeuristicsHandler, false);
			}
		}

		return true;
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
