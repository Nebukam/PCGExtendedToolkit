// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "..\..\..\Public\Graph\Pathfinding\PCGExSampleGraphEdges.h"

#include "NavigationSystem.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Splines/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"
#include "Splines/SubPoints/Orient/PCGExSubPointsOrientAverage.h"

#define LOCTEXT_NAMESPACE "PCGExSampleGraphEdgesElement"
#define PCGEX_NAMESPACE SampleGraphEdges

UPCGExSampleGraphEdgesSettings::UPCGExSampleGraphEdgesSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GoalPicker = EnsureOperation<UPCGExGoalPickerRandom>(GoalPicker);
	Blending = EnsureOperation<UPCGExSubPointsBlendInterpolate>(Blending);
}

FPCGElementPtr UPCGExSampleGraphEdgesSettings::CreateElement() const { return MakeShared<FPCGExSampleGraphEdgesElement>(); }

FPCGExSampleGraphEdgesContext::~FPCGExSampleGraphEdgesContext()
{
	PCGEX_TERMINATE_ASYNC
}

PCGEX_INITIALIZE_CONTEXT(SampleGraphEdges)

bool FPCGExSampleGraphEdgesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	const FPCGExSampleGraphEdgesContext* Context = static_cast<FPCGExSampleGraphEdgesContext*>(InContext);
	const UPCGExSampleGraphEdgesSettings* Settings = InContext->GetInputSettings<UPCGExSampleGraphEdgesSettings>();
	check(Settings);

	return true;
}

bool FPCGExSampleGraphEdgesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleGraphEdgesElement::Execute);

	PCGEX_CONTEXT(SampleGraphEdges)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->AdvancePointsIO();
		Context->GoalPicker->PrepareForData(*Context->SeedsPoints, *Context->GoalsPoints);
		Context->SetState(PCGExMT::State_ProcessingPoints);
	}

	// For each SEED
	// For each Island
	// For each GRAPH -> Merge 


	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			auto NavMeshTask = [&](int32 InGoalIndex)
			{
				PCGExData::FPointIO& PathPoints = Context->OutputPaths->Emplace_GetRef(PointIO.GetIn(), PCGExData::EInit::NewOutput);
				Context->GetAsyncManager()->Start<FSampleIslandPathTask>(
					PointIndex, Context->CurrentIO,
					InGoalIndex, &PathPoints);
			};

			const PCGEx::FPointRef& Seed = PointIO.GetInPointRef(PointIndex);

			if (Context->GoalPicker->OutputMultipleGoals())
			{
				TArray<int32> GoalIndices;
				Context->GoalPicker->GetGoalIndices(Seed, GoalIndices);
				for (const int32 GoalIndex : GoalIndices)
				{
					if (GoalIndex < 0) { continue; }
					NavMeshTask(GoalIndex);
				}
			}
			else
			{
				const int32 GoalIndex = Context->GoalPicker->GetGoalIndex(Seed);
				if (GoalIndex < 0) { return; }
				NavMeshTask(GoalIndex);
			}
		};

		if (Context->ProcessCurrentPoints(ProcessPoint)) { Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork); }
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (Context->IsAsyncWorkComplete()) { Context->Done(); }
	}

	if (Context->IsDone())
	{
		Context->OutputPaths->OutputTo(Context, true);
	}

	return Context->IsDone();
}

bool FSampleIslandPathTask::ExecuteTask()
{
	FPCGExSampleGraphEdgesContext* Context = Manager->GetContext<FPCGExSampleGraphEdgesContext>();
	PCGEX_ASYNC_CHECKPOINT
	//FWriteScopeLock WriteLock(Context->ContextLock);

	bool bSuccess = false;
	/*
		if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Context->World))
		{
			const FPCGPoint& StartPoint = PointIO->GetInPoint(TaskInfos.Index);
			const FPCGPoint& EndPoint = Context->GoalsPoints->GetInPoint(GoalIndex);
			const FVector StartLocation = StartPoint.Transform.GetLocation();
			const FVector EndLocation = EndPoint.Transform.GetLocation();
	
			// Find the path
			FPathFindingQuery PathFindingQuery = FPathFindingQuery(
				Context->World, *Context->NavData,
				StartLocation, EndLocation, nullptr, nullptr,
				TNumericLimits<FVector::FReal>::Max(),
				Context->bRequireNavigableEndLocation);
	
			PathFindingQuery.NavAgentProperties = Context->NavAgentProperties;
	
			const FPathFindingResult Result = NavSys->FindPathSync(
				Context->NavAgentProperties, PathFindingQuery,
				Context->PathfindingMode == EPCGExPathfindingMode::Regular ? EPathFindingMode::Type::Regular : EPathFindingMode::Type::Hierarchical);
	
			if (!CanContinue()) { return false; }
	
			if (Result.Result == ENavigationQueryResult::Type::Success)
			{
				TArray<FNavPathPoint>& Points = Result.Path->GetPathPoints();
				TArray<FVector> PathLocations;
				PathLocations.Reserve(Points.Num());
	
				PathLocations.Add(StartLocation);
				for (FNavPathPoint PathPoint : Points) { PathLocations.Add(PathPoint.Location); }
				PathLocations.Add(EndLocation);
	
				PCGExMath::FPathMetrics PathHelper = PCGExMath::FPathMetrics(StartLocation);
				int32 FuseCountReduce = Context->bAddGoalToPath ? 2 : 1;
				for (int i = Context->bAddSeedToPath; i < PathLocations.Num(); i++)
				{
					FVector CurrentLocation = PathLocations[i];
					if (i > 0 && i < (PathLocations.Num() - FuseCountReduce))
					{
						if (PathHelper.IsLastWithinRange(CurrentLocation, Context->FuseDistance))
						{
							// Fuse
							PathLocations.RemoveAt(i);
							i--;
							continue;
						}
					}
	
					PathHelper.Add(CurrentLocation);
				}
	
				if (PathLocations.Num() <= 2) // include start and end
				{
					bSuccess = false;
				}
				else
				{
					///////
	
					for (FVector Location : PathLocations)
					{
						FPCGPoint& Point = PathPoints->CopyPoint(StartPoint);
						Point.Transform.SetLocation(Location);
					}
	
					TArray<FPCGPoint>& MutablePoints = PathPoints->Out->GetMutablePoints();
					TArrayView<FPCGPoint> Path = MakeArrayView(MutablePoints.GetData(), PathLocations.Num());
	
					UPCGExMetadataBlender* TempBlender = Context->Blending->CreateBlender(PathPoints->Out, Context->GoalsPoints->In);
					
					Context->Blending->BlendSubPoints(StartPoint, EndPoint, Path, PathHelper, TempBlender);
					
					TempBlender->Flush();
	
					// Remove start and/or end after blending
					if (!Context->bAddSeedToPath) { MutablePoints.RemoveAt(0); }
					if (!Context->bAddGoalToPath) { MutablePoints.Pop(); }
	
					bSuccess = true;
				}
			}
		}
	*/
	return bSuccess;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
