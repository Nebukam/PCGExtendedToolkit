// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExSampleNavmesh.h"

#include "NavigationSystem.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "Graph/Pathfinding/PCGExPathfinding.h"
#include "Graph/Pathfinding/GoalPickers/PCGExGoalPickerRandom.h"
#include "Splines/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExSampleNavmeshElement"
#define PCGEX_NAMESPACE SampleNavmesh

UPCGExSampleNavmeshSettings::UPCGExSampleNavmeshSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GoalPicker = EnsureOperation<UPCGExGoalPickerRandom>(GoalPicker);
	Blending = EnsureOperation<UPCGExSubPointsBlendInterpolate>(Blending);
}

TArray<FPCGPinProperties> UPCGExSampleNavmeshSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	FPCGPinProperties& PinPropertySeeds = PinProperties.Emplace_GetRef(PCGExPathfinding::SourceSeedsLabel, EPCGDataType::Point, false, false);

#if WITH_EDITOR
	PinPropertySeeds.Tooltip = FTEXT("Seeds points for pathfinding.");
#endif // WITH_EDITOR

	FPCGPinProperties& PinPropertyGoals = PinProperties.Emplace_GetRef(PCGExPathfinding::SourceGoalsLabel, EPCGDataType::Point, false, false);

#if WITH_EDITOR
	PinPropertyGoals.Tooltip = FTEXT("Goals points for pathfinding.");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExSampleNavmeshSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPathsOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputPathsLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPathsOutput.Tooltip = FTEXT("Paths output.");
#endif // WITH_EDITOR

	return PinProperties;
}

void UPCGExSampleNavmeshSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	GoalPicker = EnsureOperation<UPCGExGoalPickerRandom>(GoalPicker);
	Blending = EnsureOperation<UPCGExSubPointsBlendInterpolate>(Blending);
	if (GoalPicker) { GoalPicker->UpdateUserFacingInfos(); }
	if (Blending) { Blending->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

PCGExData::EInit UPCGExSampleNavmeshSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }
int32 UPCGExSampleNavmeshSettings::GetPreferredChunkSize() const { return 32; }

FName UPCGExSampleNavmeshSettings::GetMainInputLabel() const { return PCGExPathfinding::SourceSeedsLabel; }
FName UPCGExSampleNavmeshSettings::GetMainOutputLabel() const { return PCGExGraph::OutputPathsLabel; }

FPCGExSampleNavmeshContext::~FPCGExSampleNavmeshContext()
{
	PCGEX_CLEANUP_ASYNC

	PathBuffer.Empty();
	PCGEX_DELETE(GoalsPoints)
	PCGEX_DELETE(OutputPaths)
}

FPCGElementPtr UPCGExSampleNavmeshSettings::CreateElement() const { return MakeShared<FPCGExSampleNavmeshElement>(); }

PCGEX_INITIALIZE_CONTEXT(SampleNavmesh)

bool FPCGExSampleNavmeshElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNavmesh)

	if (TArray<FPCGTaggedData> Goals = Context->InputData.GetInputsByPin(PCGExPathfinding::SourceGoalsLabel);
		Goals.Num() > 0)
	{
		const FPCGTaggedData& GoalsSource = Goals[0];
		Context->GoalsPoints = PCGExData::PCGExPointIO::GetPointIO(Context, GoalsSource);
	}

	if (!Settings->NavData)
	{
		if (const UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Context->World))
		{
			ANavigationData* NavData = NavSys->GetDefaultNavDataInstance();
			Context->NavData = NavData;
		}
	}

	Context->OutputPaths = new PCGExData::FPointIOGroup();

	PCGEX_BIND_OPERATION(GoalPicker, UPCGExGoalPickerRandom)
	PCGEX_BIND_OPERATION(Blending, UPCGExSubPointsBlendInterpolate)

	PCGEX_FWD(bAddSeedToPath)
	PCGEX_FWD(bAddGoalToPath)

	PCGEX_FWD(NavAgentProperties)
	PCGEX_FWD(bRequireNavigableEndLocation)
	PCGEX_FWD(PathfindingMode)

	Context->FuseDistance = Settings->FuseDistance * Settings->FuseDistance;

	if (!Context->GoalsPoints || Context->GoalsPoints->GetNum() == 0)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing Input Goals."));
		return false;
	}

	if (!Context->NavData)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing Nav Data"));
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
		if (!Boot(Context)) { return true; }
		Context->AdvancePointsIO();
		Context->GoalPicker->PrepareForData(*Context->CurrentIO, *Context->GoalsPoints);
		Context->SetState(PCGExMT::State_ProcessingPoints);
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		auto ProcessSeed = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			auto NavMeshTask = [&](int32 InGoalIndex)
			{
				Context->BufferLock.WriteLock();
				Context->PathBuffer.Emplace_GetRef(
					PointIndex, PointIO.GetInPoint(PointIndex).Transform.GetLocation(),
					InGoalIndex, Context->GoalsPoints->GetInPoint(InGoalIndex).Transform.GetLocation());
				Context->BufferLock.WriteUnlock();
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

		if (Context->ProcessCurrentPoints(ProcessSeed)) { Context->SetState(PCGExSampleNavmesh::State_Pathfinding); }
	}

	if (Context->IsState(PCGExSampleNavmesh::State_Pathfinding))
	{
		auto ProcessPath = [&](const int32 Index)
		{
			PCGExSampleNavmesh::FPath& PathObject = Context->PathBuffer[Index];
			PathObject.PathPoints = &Context->OutputPaths->Emplace_GetRef(*Context->CurrentIO, PCGExData::EInit::NewOutput);
			Context->GetAsyncManager()->Start<FNavmeshPathTask>(PathObject.SeedIndex, Context->CurrentIO, &PathObject);
		};

		if (Context->Process(ProcessPath, Context->PathBuffer.Num()))
		{
			Context->SetAsyncState(PCGExSampleNavmesh::State_WaitingPathfinding);
		}
	}

	if (Context->IsState(PCGExSampleNavmesh::State_WaitingPathfinding))
	{
		if (Context->IsAsyncWorkComplete()) { Context->Done(); }
	}

	if (Context->IsDone())
	{
		Context->OutputPaths->OutputTo(Context, true);
	}

	return Context->IsDone();
}

bool FNavmeshPathTask::ExecuteTask()
{
	PCGEX_ASYNC_CHECKPOINT

	FPCGExSampleNavmeshContext* Context = Manager->GetContext<FPCGExSampleNavmeshContext>();

	bool bSuccess = false;

	if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Context->World))
	{
		const FPCGPoint* Seed = Context->CurrentIO->TryGetInPoint(Path->SeedIndex);
		const FPCGPoint* Goal = Context->GoalsPoints->TryGetInPoint(Path->GoalIndex);

		if (!Seed || !Goal) { return false; }

		const FVector StartLocation = Seed->Transform.GetLocation();
		const FVector EndLocation = Goal->Transform.GetLocation();

		// Find the path
		FPathFindingQuery PathFindingQuery = FPathFindingQuery(
			Context->World, *Context->NavData,
			StartLocation, EndLocation, nullptr, nullptr,
			TNumericLimits<FVector::FReal>::Max(),
			Context->bRequireNavigableEndLocation);

		PathFindingQuery.NavAgentProperties = Context->NavAgentProperties;

		PCGEX_ASYNC_CHECKPOINT

		const FPathFindingResult Result = NavSys->FindPathSync(
			Context->NavAgentProperties, PathFindingQuery,
			Context->PathfindingMode == EPCGExNavmeshPathfindingMode::Regular ? EPathFindingMode::Type::Regular : EPathFindingMode::Type::Hierarchical);

		PCGEX_ASYNC_CHECKPOINT

		if (Result.Result == ENavigationQueryResult::Type::Success)
		{
			const TArray<FNavPathPoint>& Points = Result.Path->GetPathPoints();
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
				PCGEX_ASYNC_CHECKPOINT

				UPCGPointData* OutData = Path->PathPoints->GetOut();

				const int32 NumPositions = PathLocations.Num();
				const int32 LastPosition = NumPositions - 1;
				TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();
				MutablePoints.SetNum(NumPositions);

				PCGEX_ASYNC_CHECKPOINT

				FVector Location;
				for (int i = 0; i < LastPosition; i++)
				{
					Location = PathLocations[i];
					(MutablePoints[i] = *Seed).Transform.SetLocation(Location);
					Path->Metrics.Add(Location);
				}

				Location = PathLocations[LastPosition];
				(MutablePoints[LastPosition] = *Goal).Transform.SetLocation(Location);
				Path->Metrics.Add(Location);

				//

				PCGEX_ASYNC_CHECKPOINT

				const PCGExDataBlending::FMetadataBlender* TempBlender = Context->Blending->CreateBlender(
					OutData, Context->GoalsPoints->GetIn(),
					Path->PathPoints->GetOutKeys(), Context->GoalsPoints->GetInKeys());

				TArrayView<FPCGPoint> View(MutablePoints);
				Context->Blending->BlendSubPoints(View, Path->Metrics, TempBlender);

				PCGEX_DELETE(TempBlender)

				if (!Context->bAddSeedToPath) { MutablePoints.RemoveAt(0); }
				if (!Context->bAddGoalToPath) { MutablePoints.Pop(); }

				bSuccess = true;
			}
		}
	}

	return bSuccess;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
