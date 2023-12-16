// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingEdges.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "Graph/Pathfinding/PCGExPathfinding.h"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"

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
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

FPCGElementPtr UPCGExPathfindingEdgesSettings::CreateElement() const { return MakeShared<FPCGExPathfindingEdgesElement>(); }

FPCGExPathfindingEdgesContext::~FPCGExPathfindingEdgesContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE_TARRAY(SeedGoalPairs, PCGExPathfinding::FPathInfos)
}

PCGEX_INITIALIZE_CONTEXT(PathfindingEdges)

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
			PCGEX_DELETE_TARRAY(Context->SeedGoalPairs, PCGExPathfinding::FPathInfos)
			Context->GoalPicker->PrepareForData(*Context->SeedsPoints, *Context->GoalsPoints);
			Context->SetState(PCGExMT::State_ProcessingPoints);
		}
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		if (PCGExPathfinding::ProcessGoals(
			Context, Context->SeedsPoints, Context->GoalPicker, [&](const int32 SeedIndex, int32 GoalIndex)
			{
				FWriteScopeLock WriteLock(Context->ContextLock);
				Context->SeedGoalPairs.Add(
					new PCGExPathfinding::FPathInfos(
						SeedIndex, Context->CurrentIO->GetInPoint(SeedIndex).Transform.GetLocation(),
						GoalIndex, Context->GoalsPoints->GetInPoint(GoalIndex).Transform.GetLocation()));
			}))
		{
			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		if (!Context->AdvanceEdges()) { Context->SetState(PCGExMT::State_ReadyForNextPoints); }
		else { Context->SetState(PCGExGraph::State_ProcessingEdges); }
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		auto NavMeshTask = [&](int32 Index)
		{
			Context->GetAsyncManager()->Start<FSampleMeshPathTask>(Index, Context->CurrentIO, Context->SeedGoalPairs[Index]);
		};

		if (Context->Process(NavMeshTask, Context->SeedGoalPairs.Num()))
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
	PCGEX_ASYNC_CHECKPOINT

	const FPCGPoint& StartPoint = PointIO->GetInPoint(Infos->SeedIndex);
	const FPCGPoint& EndPoint = Context->GoalsPoints->GetInPoint(Infos->GoalIndex);

	const PCGExMesh::FMesh* Mesh = Context->CurrentMesh;
	const int32 StartIndex = Mesh->FindClosestVertex(Infos->StartPosition);
	const int32 EndIndex = Mesh->FindClosestVertex(Infos->EndPosition);

	return false;

	//PCGExData::FPointIO& PathPoints = Context->OutputPaths->Emplace_GetRef(Context->GetCurrentIn(), PCGExData::EInit::NewOutput);
	// Remove start and/or end after blending
	//if (!Context->bAddSeedToPath) { MutablePoints.RemoveAt(0); }
	//if (!Context->bAddGoalToPath) { MutablePoints.Pop(); }
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
