// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExSampleNavmesh.h"

#include "NavigationSystem.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Splines/SubPoints/DataBlending/PCGExSubPointsDataBlendLerp.h"
#include "Splines/SubPoints/Orient/PCGExSubPointsOrientAverage.h"

#define LOCTEXT_NAMESPACE "PCGExSampleNavmeshElement"

UPCGExSampleNavmeshSettings::UPCGExSampleNavmeshSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GoalPicker = EnsureInstruction<UPCGExGoalPickerRandom>(GoalPicker);
	Orientation = EnsureInstruction<UPCGExSubPointsOrientAverage>(Orientation);
	Blending = EnsureInstruction<UPCGExSubPointsDataBlendLerp>(Blending);
}

TArray<FPCGPinProperties> UPCGExSampleNavmeshSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	FPCGPinProperties& PinPropertySeeds = PinProperties.Emplace_GetRef(PCGExPathfinding::SourceSeedsLabel, EPCGDataType::Point, false, false);

#if WITH_EDITOR
	PinPropertySeeds.Tooltip = LOCTEXT("PCGExSourceSeedsPinTooltip", "Seeds points for pathfinding.");
#endif // WITH_EDITOR

	FPCGPinProperties& PinPropertyGoals = PinProperties.Emplace_GetRef(PCGExPathfinding::SourceGoalsLabel, EPCGDataType::Point, false, false);

#if WITH_EDITOR
	PinPropertyGoals.Tooltip = LOCTEXT("PCGExSourcGoalsPinTooltip", "Goals points for pathfinding.");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExSampleNavmeshSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPathsOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputPathsLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPathsOutput.Tooltip = LOCTEXT("PCGExOutputPathsTooltip", "Paths output.");
#endif // WITH_EDITOR

	return PinProperties;
}

void UPCGExSampleNavmeshSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (GoalPicker) { GoalPicker->UpdateUserFacingInfos(); }
	if (Orientation) { Orientation->UpdateUserFacingInfos(); }
	if (Blending) { Blending->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

PCGExPointIO::EInit UPCGExSampleNavmeshSettings::GetPointOutputInitMode() const { return PCGExPointIO::EInit::NoOutput; }
int32 UPCGExSampleNavmeshSettings::GetPreferredChunkSize() const { return 32; }

FName UPCGExSampleNavmeshSettings::GetMainPointsInputLabel() const { return PCGExPathfinding::SourceSeedsLabel; }
FName UPCGExSampleNavmeshSettings::GetMainPointsOutputLabel() const { return PCGExGraph::OutputGraphsLabel; }

FPCGElementPtr UPCGExSampleNavmeshSettings::CreateElement() const { return MakeShared<FPCGExSampleNavmeshElement>(); }

FPCGContext* FPCGExSampleNavmeshElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExSampleNavmeshContext* Context = new FPCGExSampleNavmeshContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExSampleNavmeshSettings* Settings = Context->GetInputSettings<UPCGExSampleNavmeshSettings>();
	check(Settings);

	if (TArray<FPCGTaggedData> Goals = Context->InputData.GetInputsByPin(PCGExPathfinding::SourceGoalsLabel);
		Goals.Num() > 0)
	{
		const FPCGTaggedData& GoalsSource = Goals[0];
		Context->GoalsPoints = PCGExPointIO::TryGetPointIO(Context, GoalsSource);
	}

	if (!Settings->NavData)
	{
		if (const UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Context->World))
		{
			ANavigationData* NavData = NavSys->GetDefaultNavDataInstance();
			Context->NavData = NavData;
		}
	}

	Context->OutputPaths = NewObject<UPCGExPointIOGroup>();

	Context->GoalPicker = Settings->EnsureInstruction<UPCGExGoalPickerRandom>(Settings->GoalPicker, Context);
	Context->Orientation = Settings->EnsureInstruction<UPCGExSubPointsOrientAverage>(Settings->Orientation, Context);
	Context->Blending = Settings->EnsureInstruction<UPCGExSubPointsDataBlendLerp>(Settings->Blending, Context);

	Context->bAddSeedToPath = Settings->bAddSeedToPath;
	Context->bAddGoalToPath = Settings->bAddGoalToPath;

	Context->NavAgentProperties = Settings->NavAgentProperties;
	Context->bRequireNavigableEndLocation = Settings->bRequireNavigableEndLocation;
	Context->PathfindingMode = Settings->PathfindingMode;

	Context->FuseDistance = Settings->FuseDistance * Settings->FuseDistance;


	return Context;
}

bool FPCGExSampleNavmeshElement::Validate(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Validate(InContext)) { return false; }

	const FPCGExSampleNavmeshContext* Context = static_cast<FPCGExSampleNavmeshContext*>(InContext);
	const UPCGExSampleNavmeshSettings* Settings = InContext->GetInputSettings<UPCGExSampleNavmeshSettings>();
	check(Settings);

	if (!Context->GoalsPoints || Context->GoalsPoints->In->GetPoints().Num() == 0)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingGoals", "Missing Input Goals."));
		return false;
	}

	if (!Context->NavData)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoNavData", "Missing Nav Data"));
		return false;
	}

	return true;
}

bool FPCGExSampleNavmeshElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNavmeshElement::Execute);

	FPCGExSampleNavmeshContext* Context = static_cast<FPCGExSampleNavmeshContext*>(InContext);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
		Context->AdvancePointsIO();

		Context->GoalPicker->PrepareForData(Context->CurrentIO->In, Context->GoalsPoints->In);
		Context->Orientation->PrepareForData(Context->CurrentIO);
		Context->Blending->PrepareForData(Context->CurrentIO); //TODO : Merge goals metadata with into seed meta

		Context->SetState(PCGExMT::State_ProcessingPoints);
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		auto ProcessPoint = [&](const int32 PointIndex, const UPCGExPointIO* PointIO)
		{
			if (Context->GoalPicker->OutputMultipleGoals())
			{
				TArray<int32> GoalIndices;
				Context->GoalPicker->GetGoalIndices(PointIO->GetInPoint(PointIndex), GoalIndices);
				for (const int32 GoalIndex : GoalIndices)
				{
					if (GoalIndex < 0) { continue; }

					FAsyncTask<FNavmeshPathTask>* AsyncTask = Context->CreateTask<FNavmeshPathTask>(PointIndex, PointIO->GetInPoint(PointIndex).MetadataEntry);
					FNavmeshPathTask& Task = AsyncTask->GetTask();
					Task.GoalIndex = GoalIndex;
					Task.PathPoints = Context->OutputPaths->Emplace_GetRef(PointIO->In, PCGExPointIO::EInit::NewOutput);

					Context->StartTask(AsyncTask);
				}
			}
			else
			{
				const int32 GoalIndex = Context->GoalPicker->GetGoalIndex(PointIO->GetInPoint(PointIndex), PointIndex);
				if (GoalIndex < 0) { return; }

				FAsyncTask<FNavmeshPathTask>* AsyncTask = Context->CreateTask<FNavmeshPathTask>(PointIndex, PointIO->GetInPoint(PointIndex).MetadataEntry);
				FNavmeshPathTask& Task = AsyncTask->GetTask();
				Task.GoalIndex = GoalIndex;
				Task.PathPoints = Context->OutputPaths->Emplace_GetRef(PointIO->In, PCGExPointIO::EInit::NewOutput);

				Context->StartTask(AsyncTask);
			}
		};

		if (Context->ProcessCurrentPoints(ProcessPoint))
		{
			Context->SetState(PCGExMT::State_WaitingOnAsyncWork);
		}
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (Context->IsAsyncWorkComplete())
		{
			Context->OutputPaths->OutputTo(Context, true);
			return true;
		}
	}

	return false;
}

void FNavmeshPathTask::ExecuteTask()
{
	FPCGExSampleNavmeshContext* Context = static_cast<FPCGExSampleNavmeshContext*>(TaskContext);

	if (!IsTaskValid()) { return; }

	FWriteScopeLock WriteLock(Context->ContextLock);

	bool bSuccess = false;

	if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Context->World))
	{
		const FPCGPoint& StartPoint = PointData->GetInPoint(Infos.Index);
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

		if (!IsTaskValid()) { return; }

		if (Result.Result == ENavigationQueryResult::Type::Success)
		{
			TArray<FNavPathPoint>& Points = Result.Path->GetPathPoints();
			TArray<FVector> PathLocations;
			PathLocations.Reserve(Points.Num() + 2);

			PathLocations.Add(StartLocation);
			for (FNavPathPoint PathPoint : Points) { PathLocations.Add(PathPoint.Location); }
			PathLocations.Add(EndLocation);

			double PathLength = 0;
			for (int i = 0; i < PathLocations.Num(); i++)
			{
				double Dist = 0;
				FVector CurrentLocation = PathLocations[i];
				if (i > 0)
				{
					Dist = FVector::DistSquared(CurrentLocation, PathLocations[i - 1]);
					if (i < (PathLocations.Num() - 2) && Dist < Context->FuseDistance)
					{
						// Fuse
						PathLocations.RemoveAt(i);
						i--;
						continue;
					}
				}
				PathLength += Dist;
			}

			// TODO : Seed Indices start at 0 ... Num, Goal indices start at Num ... Goal Num >
			// SubPoints processors use a view on indices to process, excluding start and end points.
			// Thus, start and end don't need to be connected

			if (PathLocations.Num() <= 2) // include start and end
			{
				bSuccess = false;
			}
			else
			{
				///////

				int32 NumPts = PathLocations.Num();
				for (int i = 0; i < NumPts; i++)
				{
					double Lerp = static_cast<double>(i) / static_cast<double>(NumPts);
					FPCGPoint& Point = PathPoints->CopyPoint(i == 0 ? StartPoint : EndPoint);
				}

				TArray<FPCGPoint>& MutablePoints = PointData->Out->GetMutablePoints();
				TArrayView<FPCGPoint> Path = MakeArrayView(MutablePoints.GetData(), NumPts);

				Context->Orientation->ProcessSubPoints(StartPoint, EndPoint, Path, PathLength);
				Context->Blending->ProcessSubPoints(StartPoint, EndPoint, Path, PathLength);

				// Remove start and/or end after blending
				if (!Context->bAddSeedToPath) { PathLocations.RemoveAt(0); }
				if (!Context->bAddGoalToPath) { PathLocations.Pop(); }

				bSuccess = true;
			}
		}
	}

	ExecutionComplete(bSuccess);
}

#undef LOCTEXT_NAMESPACE
