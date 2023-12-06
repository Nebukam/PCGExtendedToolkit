// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExSampleGraphPatches.h"

#include "NavigationSystem.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Splines/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"
#include "Splines/SubPoints/Orient/PCGExSubPointsOrientAverage.h"

#define LOCTEXT_NAMESPACE "PCGExSampleGraphPatchesElement"

UPCGExSampleGraphPatchesSettings::UPCGExSampleGraphPatchesSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GoalPicker = EnsureInstruction<UPCGExGoalPickerRandom>(GoalPicker);
	Blending = EnsureInstruction<UPCGExSubPointsBlendInterpolate>(Blending);
}

FPCGElementPtr UPCGExSampleGraphPatchesSettings::CreateElement() const { return MakeShared<FPCGExSampleGraphPatchesElement>(); }

FPCGContext* FPCGExSampleGraphPatchesElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExSampleGraphPatchesContext* Context = new FPCGExSampleGraphPatchesContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExSampleGraphPatchesSettings* Settings = Context->GetInputSettings<UPCGExSampleGraphPatchesSettings>();
	check(Settings);

	return Context;
}

bool FPCGExSampleGraphPatchesElement::Validate(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Validate(InContext)) { return false; }

	const FPCGExSampleGraphPatchesContext* Context = static_cast<FPCGExSampleGraphPatchesContext*>(InContext);
	const UPCGExSampleGraphPatchesSettings* Settings = InContext->GetInputSettings<UPCGExSampleGraphPatchesSettings>();
	check(Settings);

	return true;
}

bool FPCGExSampleGraphPatchesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleGraphPatchesElement::Execute);

	FPCGExSampleGraphPatchesContext* Context = static_cast<FPCGExSampleGraphPatchesContext*>(InContext);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
		Context->AdvancePointsIO();
		Context->GoalPicker->PrepareForData(Context->CurrentIO->In, Context->GoalsPoints->In);
		Context->Blending->PrepareForData(Context->CurrentIO->In, Context->GoalsPoints->In);
		Context->SetState(PCGExMT::State_ProcessingPoints);
	}

// For each SEED
	// For each PATCH
		// For each GRAPH -> Merge 
		
	
	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		auto ProcessPoint = [&](const int32 PointIndex, const UPCGExPointIO* PointIO)
		{
			auto NavMeshTask = [&](int32 InGoalIndex)
			{
				UPCGExPointIO* PathPoints = Context->OutputPaths->Emplace_GetRef(PointIO->In, PCGExPointIO::EInit::NewOutput);
				Context->GetAsyncManager()->StartTask<FSamplePatchPathTask>(
					PointIndex, PointIO->GetInPoint(PointIndex).MetadataEntry, Context->CurrentIO,
					InGoalIndex, PathPoints);
			};

			if (Context->GoalPicker->OutputMultipleGoals())
			{
				TArray<int32> GoalIndices;
				Context->GoalPicker->GetGoalIndices(PointIO->GetInPoint(PointIndex), GoalIndices);
				for (const int32 GoalIndex : GoalIndices)
				{
					if (GoalIndex < 0) { continue; }
					NavMeshTask(GoalIndex);
				}
			}
			else
			{
				const int32 GoalIndex = Context->GoalPicker->GetGoalIndex(PointIO->GetInPoint(PointIndex), PointIndex);
				if (GoalIndex < 0) { return; }
				NavMeshTask(GoalIndex);
			}
		};

		if (Context->ProcessCurrentPoints(ProcessPoint)) { Context->StartAsyncWait(); }
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (Context->IsAsyncWorkComplete()) { Context->StopAsyncWait(PCGExMT::State_Done); }
	}

	if (Context->IsDone())
	{
		Context->OutputPaths->OutputTo(Context, true);
		return true;
	}

	return false;
}

bool FSamplePatchPathTask::ExecuteTask()
{
	if (!CanContinue()) { return false; }

	FPCGExSampleGraphPatchesContext* Context = Manager->GetContext<FPCGExSampleGraphPatchesContext>();
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
	
				PCGExMath::FPathInfos PathHelper = PCGExMath::FPathInfos(StartLocation);
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
