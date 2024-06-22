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

UPCGExPathfindingEdgesSettings::UPCGExPathfindingEdgesSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

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
	PCGExHeuristics::THeuristicsHandler* HeuristicsHandler) const
{
	// TODO : Vtx OR/AND edge points

	PCGEX_SETTINGS_LOCAL(PathfindingEdges)

	const FPCGPoint& Seed = SeedsPoints->GetInPoint(Query->SeedIndex);
	const FPCGPoint& Goal = GoalsPoints->GetInPoint(Query->GoalIndex);

	const PCGExCluster::FCluster* Cluster = SearchOperation->Cluster;

	TArray<int32> Path;

	//Note: Can silently fail
	if (!SearchOperation->FindPath(
		Query->SeedPosition, &Settings->SeedPicking,
		Query->GoalPosition, &Settings->GoalPicking, HeuristicsHandler, Path))
	{
		// Failed
		return;
	}

	PCGExData::FPointIO* PathPoints = OutputPaths->Emplace_GetRef(Cluster->PointsIO->GetIn(), PCGExData::EInit::NewOutput);
	UPCGPointData* OutData = PathPoints->GetOut();

	PCGExGraph::CleanupClusterTags(PathPoints, true);
	PCGExGraph::CleanupVtxData(PathPoints);

	TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();
	const TArray<FPCGPoint>& InPoints = Cluster->PointsIO->GetIn()->GetPoints();

	MutablePoints.Reserve(Path.Num() + 2);

	if (Settings->bAddSeedToPath) { MutablePoints.Add_GetRef(Seed).MetadataEntry = PCGInvalidEntryKey; }
	for (const int32 VtxIndex : Path) { MutablePoints.Add(InPoints[Cluster->Nodes[VtxIndex].PointIndex]); }
	if (Settings->bAddGoalToPath) { MutablePoints.Add_GetRef(Goal).MetadataEntry = PCGInvalidEntryKey; }

	if (Settings->bUseSeedAttributeToTagPath) { PathPoints->Tags->RawTags.Add(SeedTagValueGetter->SoftGet(Seed, TEXT(""))); }
	if (Settings->bUseGoalAttributeToTagPath) { PathPoints->Tags->RawTags.Add(GoalTagValueGetter->SoftGet(Goal, TEXT(""))); }

	SeedForwardHandler->Forward(Query->SeedIndex, PathPoints);
	GoalForwardHandler->Forward(Query->GoalIndex, PathPoints);
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

	PCGEX_DELETE(SeedsPoints)
	PCGEX_DELETE(GoalsPoints)
	PCGEX_DELETE(OutputPaths)

	PCGEX_DELETE(SeedTagValueGetter)
	PCGEX_DELETE(GoalTagValueGetter)

	PCGEX_DELETE(SeedForwardHandler)
	PCGEX_DELETE(GoalForwardHandler)
}


bool FPCGExPathfindingEdgesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingEdges)

	PCGEX_OPERATION_BIND(GoalPicker, UPCGExGoalPickerRandom)
	PCGEX_OPERATION_BIND(SearchAlgorithm, UPCGExSearchAStar)


	Context->SeedsPoints = Context->TryGetSingleInput(PCGExGraph::SourceSeedsLabel, true);
	if (!Context->SeedsPoints) { return false; }

	Context->GoalsPoints = Context->TryGetSingleInput(PCGExGraph::SourceGoalsLabel, true);
	if (!Context->GoalsPoints) { return false; }

	if (Settings->bUseSeedAttributeToTagPath)
	{
		Context->SeedTagValueGetter = new PCGEx::FLocalToStringGetter();
		Context->SeedTagValueGetter->Capture(Settings->SeedTagAttribute);
		if (!Context->SeedTagValueGetter->SoftGrab(Context->SeedsPoints))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Missing specified Attribute to Tag on Seed points."));
			return false;
		}
	}

	if (Settings->bUseGoalAttributeToTagPath)
	{
		Context->GoalTagValueGetter = new PCGEx::FLocalToStringGetter();
		Context->GoalTagValueGetter->Capture(Settings->GoalTagAttribute);
		if (!Context->GoalTagValueGetter->SoftGrab(Context->GoalsPoints))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Missing specified Attribute to Tag on Goal points."));
			return false;
		}
	}

	Context->SeedForwardHandler = new PCGExDataBlending::FDataForwardHandler(&Settings->SeedForwardAttributes, Context->SeedsPoints);
	Context->GoalForwardHandler = new PCGExDataBlending::FDataForwardHandler(&Settings->GoalForwardAttributes, Context->GoalsPoints);

	Context->OutputPaths = new PCGExData::FPointIOCollection();
	Context->OutputPaths->DefaultOutputLabel = PCGExGraph::OutputPathsLabel;

	// Prepare path queries

	Context->GoalPicker->PrepareForData(Context->SeedsPoints, Context->GoalsPoints);
	PCGExPathfinding::ProcessGoals(
		Context->SeedsPoints, Context->GoalPicker,
		[&](const int32 SeedIndex, const int32 GoalIndex)
		{
			Context->PathQueries.Add(
				new PCGExPathfinding::FPathQuery(
					SeedIndex, Context->SeedsPoints->GetInPoint(SeedIndex).Transform.GetLocation(),
					GoalIndex, Context->GoalsPoints->GetInPoint(GoalIndex).Transform.GetLocation()));
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

	if (Context->IsDone())
	{
		Context->OutputPaths->OutputTo(Context);
		Context->ExecuteEnd();
	}

	return Context->IsDone();
}


namespace PCGExPathfindingEdge
{
	bool FSampleClusterPathTask::ExecuteTask()
	{
		const FPCGExPathfindingEdgesContext* Context = Manager->GetContext<FPCGExPathfindingEdgesContext>();
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
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathfindingEdge::FClusterProcessor::Process);

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
		SearchOperation->PrepareForCluster(Cluster);

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
			for (int i = 0; i < TypedContext->PathQueries.Num(); i++)
			{
				AsyncManagerPtr->Start<FSampleClusterPathTask>(i, VtxIO, SearchOperation, &TypedContext->PathQueries, HeuristicsHandler, false);
			}
		}

		return true;
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
