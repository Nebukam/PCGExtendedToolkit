// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingEdges.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "PCGExPathfinding.cpp"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Graph/Pathfinding/Search/PCGExSearchOperation.h"
#include "Algo/Reverse.h"

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
void FPCGExPathfindingEdgesContext::TryFindPath(const PCGExPathfinding::FPathQuery* Query) const
{
	PCGEX_SETTINGS_LOCAL(PathfindingEdges)

	const FPCGPoint& Seed = SeedsPoints->GetInPoint(Query->SeedIndex);
	const FPCGPoint& Goal = GoalsPoints->GetInPoint(Query->GoalIndex);

	const PCGExCluster::FCluster* Cluster = CurrentCluster;

	TArray<int32> Path;

	//Note: Can silently fail
	if (!SearchAlgorithm->FindPath(
		Query->SeedPosition, &Settings->SeedPicking,
		Query->GoalPosition, &Settings->GoalPicking, HeuristicsHandler, Path))
	{
		// Failed
		return;
	}

	PCGExData::FPointIO& PathPoints = OutputPaths->Emplace_GetRef(CurrentIO->GetIn(), PCGExData::EInit::NewOutput);
	UPCGPointData* OutData = PathPoints.GetOut();

	PCGExGraph::CleanupClusterTags(&PathPoints, true);
	PCGExGraph::CleanupVtxData(&PathPoints);

	TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();
	const TArray<FPCGPoint>& InPoints = CurrentIO->GetIn()->GetPoints();

	MutablePoints.Reserve(Path.Num() + 2);

	if (bAddSeedToPath) { MutablePoints.Add_GetRef(Seed).MetadataEntry = PCGInvalidEntryKey; }
	for (const int32 VtxIndex : Path) { MutablePoints.Add(InPoints[Cluster->Nodes[VtxIndex].PointIndex]); }
	if (bAddGoalToPath) { MutablePoints.Add_GetRef(Goal).MetadataEntry = PCGInvalidEntryKey; }

	if (Settings->bUseSeedAttributeToTagPath) { PathPoints.Tags->RawTags.Add(SeedTagValueGetter->SoftGet(Seed, TEXT(""))); }
	if (Settings->bUseGoalAttributeToTagPath) { PathPoints.Tags->RawTags.Add(GoalTagValueGetter->SoftGet(Goal, TEXT(""))); }

	SeedForwardHandler->Forward(Query->SeedIndex, &PathPoints);
	GoalForwardHandler->Forward(Query->GoalIndex, &PathPoints);
}

PCGEX_INITIALIZE_ELEMENT(PathfindingEdges)

PCGExData::EInit UPCGExPathfindingEdgesSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FPCGExPathfindingEdgesContext::~FPCGExPathfindingEdgesContext()
{
	PCGEX_TERMINATE_ASYNC
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

	if (!Context->ExecuteAutomation()) { return false; }

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges) { return false; }

			PCGEX_DELETE_TARRAY(Context->PathBuffer)

			Context->GoalPicker->PrepareForData(*Context->SeedsPoints, *Context->GoalsPoints);
			Context->SetState(PCGExMT::State_ProcessingPoints);
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

	auto StartEdgeProcessing = [&]()
	{
		Context->SearchAlgorithm->PrepareForCluster(Context->CurrentCluster, Context->ClusterProjection);
		Context->HeuristicsHandler->PrepareForCluster(Context->CurrentCluster);
		Context->SetAsyncState(PCGExGraph::State_ProcessingEdges);
	};

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		if (!Context->AdvanceEdges(true))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		if (!Context->CurrentCluster) { return false; }

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

		if ((Context->bWaitingOnClusterProjection = Context->SearchAlgorithm->GetRequiresProjection()))
		{
			Context->SetState(PCGExCluster::State_ProjectingCluster);
			return false;
		}

		StartEdgeProcessing();
	}

	if (Context->IsState(PCGExCluster::State_ProjectingCluster)) { StartEdgeProcessing(); }

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		PCGEX_WAIT_ASYNC

		Context->HeuristicsHandler->CompleteClusterPreparation();

		if (Context->HeuristicsHandler->HasGlobalFeedback())
		{
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
			Context->CurrentPathBufferIndex, Context->CurrentIO,
			Context->PathBuffer[Context->CurrentPathBufferIndex]);

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
		Context->PostProcessOutputs();
	}

	return Context->IsDone();
}

bool FSampleClusterPathTask::ExecuteTask()
{
	const FPCGExPathfindingEdgesContext* Context = Manager->GetContext<FPCGExPathfindingEdgesContext>();
	PCGEX_SETTINGS(PathfindingEdges)

	Context->TryFindPath(Query);

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
