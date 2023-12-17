// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfindingEdges.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "Graph/Pathfinding/PCGExPathfinding.h"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Algo/Reverse.h"

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

	PCGEX_DELETE_TARRAY(PathBuffer)
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
			PCGEX_DELETE_TARRAY(Context->PathBuffer)
			Context->GoalPicker->PrepareForData(*Context->SeedsPoints, *Context->GoalsPoints);
			Context->SetState(PCGExMT::State_ProcessingPoints);
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
			//TODO: Cache heuristics for current mesh
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
	PCGEX_ASYNC_CHECKPOINT

	const FPCGPoint& StartPoint = Context->SeedsPoints->GetInPoint(Query->SeedIndex);
	const FPCGPoint& EndPoint = Context->GoalsPoints->GetInPoint(Query->GoalIndex);

	const PCGExMesh::FMesh* Mesh = Context->CurrentMesh;

	TArray<int32> Path;

	if (!PCGExPathfinding::FindPath(
		Context->CurrentMesh, Query->StartPosition, Query->EndPosition,
		Context->Heuristics, Path))
	{
		return false;
	}

	const PCGExData::FPointIO& PathPoints = Context->OutputPaths->Emplace_GetRef(Context->GetCurrentIn(), PCGExData::EInit::NewOutput);
	UPCGPointData* OutData = PathPoints.GetOut();
	TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();
	const TArray<FPCGPoint>& InPoints = Context->GetCurrentIn()->GetPoints();

	if (Context->bAddSeedToPath) { MutablePoints.Emplace_GetRef(StartPoint).MetadataEntry = PCGInvalidEntryKey; }
	for (const int32 Index : Path) { MutablePoints.Add(InPoints[Mesh->Vertices[Index].PointIndex]); }
	if (Context->bAddGoalToPath) { MutablePoints.Emplace_GetRef(EndPoint).MetadataEntry = PCGInvalidEntryKey; }

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
