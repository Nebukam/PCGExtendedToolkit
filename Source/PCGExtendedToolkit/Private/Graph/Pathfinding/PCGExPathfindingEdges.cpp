// Copyright Timothé Lapetite 2023
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

void UPCGExPathfindingEdgesSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (GoalPicker) { GoalPicker->UpdateUserFacingInfos(); }
	HeuristicsModifiers.UpdateUserFacingInfos();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

PCGEX_INITIALIZE_ELEMENT(PathfindingEdges)

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

		if (PCGExPathfinding::ProcessGoals(
			Initialize, Context, Context->SeedsPoints, Context->GoalPicker,
			[&](const int32 SeedIndex, int32 GoalIndex)
			{
				Context->BufferLock.WriteLock();
				Context->PathBuffer.Add(
					new PCGExPathfinding::FPathQuery(
						SeedIndex, Context->SeedsPoints->GetInPoint(SeedIndex).Transform.GetLocation(),
						GoalIndex, Context->GoalsPoints->GetInPoint(GoalIndex).Transform.GetLocation()));
				Context->BufferLock.WriteUnlock();
			}))
		{
			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		if (!Context->AdvanceEdges()) { Context->SetState(PCGExMT::State_ReadyForNextPoints); }
		else
		{
			if (Context->CurrentMesh->HasInvalidEdges())
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input edges are invalid. This will highly likely cause unexpected results or failed pathfinding."));
			}
			Context->HeuristicsModifiers->PrepareForData(*Context->CurrentIO, *Context->CurrentEdges, Context->Heuristics->GetScale());
			Context->SetState(PCGExGraph::State_ProcessingEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		auto NavMeshTask = [&](int32 Index)
		{
			Context->GetAsyncManager()->Start<FSampleMeshPathTask>(Index, Context->CurrentIO, Context->PathBuffer[Index]);
		};

		if (Context->Process(NavMeshTask, Context->PathBuffer.Num()))
		{
			Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
		}
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (Context->IsAsyncWorkComplete()) { Context->SetState(PCGExGraph::State_ReadyForNextEdges); }
	}

	if (Context->IsDone())
	{
		Context->OutputPaths->OutputTo(Context, true);
	}

	return Context->IsDone();
}

bool FSampleMeshPathTask::ExecuteTask()
{
	FPCGExPathfindingEdgesContext* Context = Manager->GetContext<FPCGExPathfindingEdgesContext>();
	

	const FPCGPoint& Seed = Context->SeedsPoints->GetInPoint(Query->SeedIndex);
	const FPCGPoint& Goal = Context->GoalsPoints->GetInPoint(Query->GoalIndex);

	const PCGExMesh::FMesh* Mesh = Context->CurrentMesh;

	TArray<int32> Path;

	if (!PCGExPathfinding::FindPath(
		Context->CurrentMesh, Query->SeedPosition, Query->GoalPosition,
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
	for (const int32 VtxIndex : Path) { MutablePoints.Add(InPoints[Mesh->Vertices[VtxIndex].PointIndex]); }
	if (Context->bAddGoalToPath) { MutablePoints.Add_GetRef(Goal).MetadataEntry = PCGInvalidEntryKey; }

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
