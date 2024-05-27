// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingEdges.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "PCGExPathfinding.cpp"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Algo/Reverse.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDistance.h"
#include "Graph/Pathfinding/Search/PCGExSearchOperation.h"

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
	HeuristicsModifiers.UpdateUserFacingInfos();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGEX_INITIALIZE_ELEMENT(PathfindingEdges)

PCGExData::EInit UPCGExPathfindingEdgesSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FPCGExPathfindingEdgesContext::~FPCGExPathfindingEdgesContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(GlobalExtraWeights)
	PCGEX_DELETE_TARRAY(PathBuffer)
}


bool FPCGExPathfindingEdgesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathfindingProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingEdges)

	return true;
}

bool FPCGExPathfindingEdgesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathfindingEdgesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathfindingEdges)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points have no associated edges."));
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
			}
			else
			{
				PCGEX_DELETE_TARRAY(Context->PathBuffer)

				Context->GoalPicker->PrepareForData(*Context->SeedsPoints, *Context->GoalsPoints);
				Context->SetState(PCGExMT::State_ProcessingPoints);
			}
		}
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		PCGExPathfinding::ProcessGoals(
			Context->SeedsPoints, Context->GoalPicker,
			[&](const int32 SeedIndex, const int32 GoalIndex)
			{
				Context->PathBuffer.Add(
					new PCGExPathfinding::FPathQuery(
						SeedIndex, Context->SeedsPoints->GetInPoint(SeedIndex).Transform.GetLocation(),
						GoalIndex, Context->GoalsPoints->GetInPoint(GoalIndex).Transform.GetLocation()));
			});

		Context->SetState(PCGExGraph::State_ReadyForNextEdges);
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		if (!Context->AdvanceEdges(true))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		if (!Context->CurrentCluster)
		{
			PCGEX_INVALID_CLUSTER_LOG
			return false;
		}

		if (Settings->bUseOctreeSearch)
		{
			if (Settings->SeedPicking.PickingMethod == EPCGExClusterClosestSearchMode::Node ||
				Settings->GoalPicking.PickingMethod == EPCGExClusterClosestSearchMode::Node)
			{
				Context->CurrentCluster->RebuildOctree(EPCGExClusterClosestSearchMode::Node);
			}

			if (Settings->SeedPicking.PickingMethod == EPCGExClusterClosestSearchMode::Edge ||
				Settings->GoalPicking.PickingMethod == EPCGExClusterClosestSearchMode::Edge)
			{
				Context->CurrentCluster->RebuildOctree(EPCGExClusterClosestSearchMode::Edge);
			}
		}

		Context->SetState(PCGExCluster::State_ProjectingCluster);
	}

	if (Context->IsState(PCGExCluster::State_ProjectingCluster))
	{
		if (Context->SearchAlgorithm->GetRequiresProjection())
		{
			if (!Context->ProjectCluster()) { return false; }
		}

		Context->SearchAlgorithm->PrepareForCluster(Context->CurrentCluster, Context->ClusterProjection);
		Context->GetAsyncManager()->Start<FPCGExCompileModifiersTask>(0, Context->CurrentIO, Context->CurrentEdges, Context->HeuristicsModifiers);
		Context->SetAsyncState(PCGExGraph::State_ProcessingEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		PCGEX_WAIT_ASYNC

		PCGEX_DELETE(Context->GlobalExtraWeights)
		Context->Heuristics->PrepareForData(Context->CurrentCluster);

		if (Settings->bWeightUpVisited)
		{
			Context->GlobalExtraWeights = new PCGExPathfinding::FExtraWeights(
				Context->CurrentCluster,
				Settings->VisitedPointsWeightFactor,
				Settings->VisitedEdgesWeightFactor);

			Context->CurrentPathBufferIndex = -1;
			Context->SetAsyncState(PCGExPathfinding::State_Pathfinding);
		}
		else
		{
			for (int i = 0; i < Context->PathBuffer.Num(); i++)
			{
				Context->GetAsyncManager()->Start<FSampleClusterPathTask>(i, Context->CurrentIO, Context->PathBuffer[i]);
			}

			Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
		}
	}

	if (Context->IsState(PCGExPathfinding::State_Pathfinding))
	{
		// Advance to next plot
		Context->CurrentPathBufferIndex++;
		if (!Context->PathBuffer.IsValidIndex(Context->CurrentPathBufferIndex))
		{
			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
			return false;
		}

		Context->GetAsyncManager()->Start<FSampleClusterPathTask>(
			Context->CurrentPathBufferIndex,
			Context->CurrentIO,
			Context->PathBuffer[Context->CurrentPathBufferIndex],
			Context->GlobalExtraWeights);

		Context->SetAsyncState(PCGExPathfinding::State_WaitingPathfinding);
	}

	if (Context->IsState(PCGExPathfinding::State_WaitingPathfinding))
	{
		PCGEX_WAIT_ASYNC
		Context->SetState(PCGExPathfinding::State_Pathfinding);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC
		Context->SetState(PCGExGraph::State_ReadyForNextEdges);
	}

	if (Context->IsDone())
	{
		Context->OutputPaths->OutputTo(Context);
	}

	return Context->IsDone();
}

bool FSampleClusterPathTask::ExecuteTask()
{
	const FPCGExPathfindingEdgesContext* Context = Manager->GetContext<FPCGExPathfindingEdgesContext>();
	PCGEX_SETTINGS(PathfindingEdges)

	const FPCGPoint& Seed = Context->SeedsPoints->GetInPoint(Query->SeedIndex);
	const FPCGPoint& Goal = Context->GoalsPoints->GetInPoint(Query->GoalIndex);

	const PCGExCluster::FCluster* Cluster = Context->CurrentCluster;

	TArray<int32> Path;

	//Note: Can silently fail
	if (!Context->SearchAlgorithm->FindPath(
		Query->SeedPosition, &Settings->SeedPicking,
		Query->GoalPosition, &Settings->GoalPicking, Context->Heuristics, Context->HeuristicsModifiers, Path, GlobalExtraWeights))
	{
		// Failed
		return false;
	}

	PCGExData::FPointIO& PathPoints = Context->OutputPaths->Emplace_GetRef(Context->GetCurrentIn(), PCGExData::EInit::NewOutput);
	UPCGPointData* OutData = PathPoints.GetOut();

	PCGExGraph::CleanupVtxData(&PathPoints);

	TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();
	const TArray<FPCGPoint>& InPoints = Context->GetCurrentIn()->GetPoints();

	MutablePoints.Reserve(Path.Num() + 2);

	if (Context->bAddSeedToPath) { MutablePoints.Add_GetRef(Seed).MetadataEntry = PCGInvalidEntryKey; }
	for (const int32 VtxIndex : Path) { MutablePoints.Add(InPoints[Cluster->Nodes[VtxIndex].PointIndex]); }
	if (Context->bAddGoalToPath) { MutablePoints.Add_GetRef(Goal).MetadataEntry = PCGInvalidEntryKey; }

	if (Settings->bUseSeedAttributeToTagPath) { PathPoints.Tags->RawTags.Add(Context->SeedTagValueGetter->SoftGet(Seed, TEXT(""))); }
	if (Settings->bUseGoalAttributeToTagPath) { PathPoints.Tags->RawTags.Add(Context->GoalTagValueGetter->SoftGet(Goal, TEXT(""))); }

	PathPoints.Flatten();

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
