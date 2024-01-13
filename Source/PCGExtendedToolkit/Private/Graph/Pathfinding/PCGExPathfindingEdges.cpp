// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingEdges.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "PCGExPathfinding.cpp"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Algo/Reverse.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicDistance.h"

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
	HeuristicsModifiers.UpdateUserFacingInfos();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

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

	PCGEX_CONTEXT(PathfindingEdges)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvanceAndBindPointsIO()) { Context->Done(); }
		else
		{
			if (!Context->BoundEdges->IsValid())
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
		auto Initialize = []()
		{
		};

		if (!PCGExPathfinding::ProcessGoals(
			Initialize, Context, Context->SeedsPoints, Context->GoalPicker,
			[&](const int32 SeedIndex, const int32 GoalIndex)
			{
				Context->BufferLock.WriteLock();
				Context->PathBuffer.Add(
					new PCGExPathfinding::FPathQuery(
						SeedIndex, Context->SeedsPoints->GetInPoint(SeedIndex).Transform.GetLocation(),
						GoalIndex, Context->GoalsPoints->GetInPoint(GoalIndex).Transform.GetLocation()));
				Context->BufferLock.WriteUnlock();
			}))
		{
			return false;
		}

		Context->SetState(PCGExGraph::State_ReadyForNextEdges);
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		if (!Context->AdvanceEdges()) { Context->SetState(PCGExMT::State_ReadyForNextPoints); }
		else
		{
			if (Context->CurrentCluster->HasInvalidEdges())
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input edges are invalid. This will highly likely cause unexpected results or failed pathfinding."));
			}
			Context->HeuristicsModifiers->PrepareForData(*Context->CurrentIO, *Context->CurrentEdges, Context->Heuristics->GetScale());
			Context->SetState(PCGExGraph::State_ProcessingEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		auto SampleClusterTask = [&](const int32 Index)
		{
			Context->GetAsyncManager()->Start<FSampleClusterPathTask>(Index, Context->CurrentIO, Context->PathBuffer[Index]);
		};

		if (!Context->Process(SampleClusterTask, Context->PathBuffer.Num())) { return false; }
		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
		Context->SetState(PCGExGraph::State_ReadyForNextEdges);
	}

	if (Context->IsDone())
	{
		Context->OutputPaths->OutputTo(Context, true);
	}

	return Context->IsDone();
}

bool FSampleClusterPathTask::ExecuteTask()
{
	const FPCGExPathfindingEdgesContext* Context = Manager->GetContext<FPCGExPathfindingEdgesContext>();

	const FPCGPoint& Seed = Context->SeedsPoints->GetInPoint(Query->SeedIndex);
	const FPCGPoint& Goal = Context->GoalsPoints->GetInPoint(Query->GoalIndex);

	const PCGExCluster::FCluster* Cluster = Context->CurrentCluster;

	TArray<int32> Path;

	if (!PCGExPathfinding::FindPath(
		Context->CurrentCluster, Query->SeedPosition, Query->GoalPosition,
		Context->Heuristics, Context->HeuristicsModifiers, Path))
	{
		return false;
	}

	const PCGExData::FPointIO& PathPoints = Context->OutputPaths->Emplace_GetRef(Context->GetCurrentIn(), PCGExData::EInit::NewOutput);
	UPCGPointData* OutData = PathPoints.GetOut();
	TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();
	const TArray<FPCGPoint>& InPoints = Context->GetCurrentIn()->GetPoints();

	MutablePoints.Reserve(Path.Num() + 2);

	if (Context->bAddSeedToPath) { MutablePoints.Add_GetRef(Seed).MetadataEntry = PCGInvalidEntryKey; }
	for (const int32 VtxIndex : Path) { MutablePoints.Add(InPoints[Cluster->Vertices[VtxIndex].PointIndex]); }
	if (Context->bAddGoalToPath) { MutablePoints.Add_GetRef(Goal).MetadataEntry = PCGInvalidEntryKey; }

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
