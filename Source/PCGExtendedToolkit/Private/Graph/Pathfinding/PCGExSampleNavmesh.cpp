// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExSampleNavmesh.h"

#include "NavigationSystem.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"

#define LOCTEXT_NAMESPACE "PCGExSampleNavmeshElement"

UPCGExSampleNavmeshSettings::UPCGExSampleNavmeshSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GoalPicker = EnsureInstruction<UPCGExGoalPickerRandom>(GoalPicker);
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
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

PCGExIO::EInitMode UPCGExSampleNavmeshSettings::GetPointOutputInitMode() const { return PCGExIO::EInitMode::NoOutput; }
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
		Context->GoalsPoints = PCGExIO::TryGetPointIO(Context, GoalsSource);
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

	Context->GoalPicker = Settings->EnsureInstruction<UPCGExGoalPickerRandom>(Settings->GoalPicker);
	Context->PointsOrientation = Settings->PointsOrientation;
	Context->bAddSeedToPath = Settings->bAddSeedToPath;
	Context->bAddGoalToPath = Settings->bAddGoalToPath;
	Context->LerpMode = Settings->LerpMode;

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
					Task.PathPoints = Context->OutputPaths->Emplace_GetRef(PointIO->In, PCGExIO::EInitMode::NewOutput);

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
				Task.PathPoints = Context->OutputPaths->Emplace_GetRef(PointIO->In, PCGExIO::EInitMode::NewOutput);

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
			PathLocations.Reserve(Points.Num() + Context->bAddSeedToPath + Context->bAddGoalToPath);

			if (Context->bAddSeedToPath) { PathLocations.Add(StartLocation); }
			for (FNavPathPoint PathPoint : Points) { PathLocations.Add(PathPoint.Location); }
			if (Context->bAddGoalToPath) { PathLocations.Add(EndLocation); }

			double PathLength = 0;
			int32 NumPts = PathLocations.Num() - 1;
			for (int i = 0; i <= NumPts; i++)
			{
				double Dist = 0;
				FVector CurrentLocation = PathLocations[i];
				if (i > 0)
				{
					Dist = FVector::DistSquared(CurrentLocation, PathLocations[i - 1]);
					if (i < NumPts && Dist < Context->FuseDistance)
					{
						// Fuse
						PathLocations.RemoveAt(i);
						i--;
						NumPts--;
						continue;
					}
				}
				PathLength += Dist;
			}


			NumPts = PathLocations.Num() - 1;
			if (NumPts <= 0)
			{
				bSuccess = false;
			}
			else
			{
				FVector PrevPos = FVector::ZeroVector;
				double CurrentPathLength = 0;
				bool bDistanceBasedLerp = Context->LerpMode == EPCGExPointLerpMode::Distance;
				for (int i = 0; i <= NumPts; i++)
				{
					double Lerp = static_cast<double>(i) / static_cast<double>(NumPts);
					FPCGPoint& Point = PathPoints->NewPoint(StartPoint);
					//Point.Seed = StartPoint.Seed;

					FVector CurrentLocation = PathLocations[i];

					if (i > 0)
					{
						if (bDistanceBasedLerp)
						{
							CurrentPathLength += FVector::DistSquared(CurrentLocation, PrevPos);
							Lerp = CurrentPathLength / PathLength;
						}

						PCGExMath::Lerp(StartPoint, EndPoint, Point, Lerp);
						//Context->InputAttributeMap.SetLerp(StartPoint.MetadataEntry, EndPoint.MetadataEntry, Point.MetadataEntry, Lerp);
					}

					Point.Transform.SetLocation(CurrentLocation);

					if (i < NumPts)
					{
						if (i == 0) // First point
						{
							Context->PointsOrientation.OrientNextOnly(Point.Transform, PathLocations[i + 1]);
						}
						else // Regular point
						{
							Context->PointsOrientation.Orient(Point.Transform, PathLocations[i - 1], PathLocations[i + 1]);
						}
					}
					else if (i >= 1) // Last point
					{
						Context->PointsOrientation.OrientPreviousOnly(Point.Transform, PathLocations[i - 1]);
					}

					PrevPos = CurrentLocation;
				}

				bSuccess = true;
			}
		}
	}

	ExecutionComplete(bSuccess);
}

#undef LOCTEXT_NAMESPACE
