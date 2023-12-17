// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMT.h"
#include "PCGExPointsProcessor.h"
#include "GoalPickers/PCGExGoalPicker.h"
#include "Graph/PCGExMesh.h"
#include "Heuristics/PCGExHeuristicOperation.h"

#include "PCGExPathfinding.generated.h"


UENUM(BlueprintType)
enum class EPCGExPathfindingGoalPickMethod : uint8
{
	SeedIndex UMETA(DisplayName = "Seed Index", Tooltip="Uses the seed index as goal index."),
	LocalAttribute UMETA(DisplayName = "Attribute", Tooltip="Uses a local attribute of the seed as goal index. Value is wrapped."),
	RandomPick UMETA(DisplayName = "Random Pick", Tooltip="Picks the goal randomly."),
	MultipleLocalAttribute UMETA(DisplayName = "Attribute (Multiple)", Tooltip="Uses a multiple local attribute of the seed as goal indices. Each seed will create multiple paths."),
	All UMETA(DisplayName = "All", Tooltip="Each seed will create a path for each goal."),
};

UENUM(BlueprintType)
enum class EPCGExPathPointOrientation : uint8
{
	None UMETA(DisplayName = "None", Tooltip="No orientation is applied to the point"),
	Average UMETA(DisplayName = "Average", Tooltip="Orientation is averaged between previous and next point."),
	Weighted UMETA(DisplayName = "Weighted", Tooltip="Orientation is weighted based on distance."),
	WeightedInverse UMETA(DisplayName = "Weighted (Inverse)", Tooltip="Same as Weighted, but weights are swapped."),
	LookAtNext UMETA(DisplayName = "Look at Next", Tooltip="Orientation is set so the point forward axis looks at the next point"),
};

namespace PCGExPathfinding
{
	const FName SourceSeedsLabel = TEXT("Seeds");
	const FName SourceGoalsLabel = TEXT("Goals");

	constexpr PCGExMT::AsyncState State_Pathfinding = __COUNTER__;
	constexpr PCGExMT::AsyncState State_WaitingPathfinding = __COUNTER__;

	struct PCGEXTENDEDTOOLKIT_API FPathQuery
	{
		FPathQuery(const int32 InSeedIndex, const FVector& InSeedPosition,
		           const int32 InGoalIndex, const FVector& InGoalPosition):
			SeedIndex(InSeedIndex), SeedPosition(InSeedPosition),
			GoalIndex(InGoalIndex), GoalPosition(InGoalPosition)
		{
		}

		int32 SeedIndex = -1;
		FVector SeedPosition;
		int32 GoalIndex = -1;
		FVector GoalPosition;
	};

	template <class InitializeFunc>
	static bool ProcessGoals(
		InitializeFunc&& Initialize,
		FPCGExPointsProcessorContext* Context,
		const PCGExData::FPointIO* SeedIO,
		const UPCGExGoalPicker* GoalPicker,
		TFunction<void(int32, int32)>&& GoalFunc)
	{
		return Context->Process(
			Initialize,
			[&](const int32 PointIndex)
			{
				const PCGEx::FPointRef& Seed = SeedIO->GetInPointRef(PointIndex);

				if (GoalPicker->OutputMultipleGoals())
				{
					TArray<int32> GoalIndices;
					GoalPicker->GetGoalIndices(Seed, GoalIndices);
					for (const int32 GoalIndex : GoalIndices)
					{
						if (GoalIndex < 0) { continue; }
						GoalFunc(PointIndex, GoalIndex);
					}
				}
				else
				{
					const int32 GoalIndex = GoalPicker->GetGoalIndex(Seed);
					if (GoalIndex < 0) { return; }
					GoalFunc(PointIndex, GoalIndex);
				}
			}, SeedIO->GetNum());
	}

	static bool FindPath(
		const PCGExMesh::FMesh* Mesh,
		const int32 Seed, const int32 Goal,
		const UPCGExHeuristicOperation* Heuristics,
		TArray<int32>& OutPath)
	{
		if (Seed == Goal) { return false; }

		const PCGExMesh::FVertex& StartVtx = Mesh->Vertices[Seed];
		const PCGExMesh::FVertex& EndVtx = Mesh->Vertices[Goal];

		// Basic A* implementation
		TMap<int32, double> CachedScores;

		TArray<PCGExMesh::FScoredVertex*> OpenList;
		OpenList.Reserve(Mesh->Vertices.Num() / 3);

		TArray<PCGExMesh::FScoredVertex*> ClosedList;
		ClosedList.Reserve(Mesh->Vertices.Num() / 3);
		TSet<int32> Visited;

		OpenList.Add(new PCGExMesh::FScoredVertex(StartVtx, 0));
		bool bSuccess = false;

		while (!bSuccess && !OpenList.IsEmpty())
		{
			PCGExMesh::FScoredVertex* CurrentWVtx = OpenList.Pop();
			const int32 CurrentVtxIndex = CurrentWVtx->Vertex->MeshIndex;

			ClosedList.Add(CurrentWVtx);
			Visited.Add(CurrentVtxIndex);

			if (CurrentVtxIndex == EndVtx.MeshIndex)
			{
				bSuccess = true;
				TArray<int32> Path;

				while (CurrentWVtx)
				{
					Path.Add(CurrentWVtx->Vertex->MeshIndex);
					CurrentWVtx = CurrentWVtx->From;
				}

				Algo::Reverse(Path);
				OutPath.Append(Path);
			}
			else
			{
				//Get current index neighbors
				for (const PCGExMesh::FVertex& Vtx = Mesh->GetVertex(CurrentVtxIndex);
				     const int32 OtherVtxIndex : Vtx.Neighbors) //TODO: Use edge instead?
				{
					if (Visited.Contains(OtherVtxIndex)) { continue; }

					const PCGExMesh::FVertex& OtherVtx = Mesh->GetVertex(OtherVtxIndex);
					double Score = Heuristics->ComputeScore(CurrentWVtx, OtherVtx, StartVtx, EndVtx);

					if (const double* PreviousScore = CachedScores.Find(OtherVtxIndex);
						PreviousScore && !Heuristics->IsBetterScore(*PreviousScore, Score))
					{
						continue;
					}

					PCGExMesh::FScoredVertex* NewWVtx = new PCGExMesh::FScoredVertex(OtherVtx, Score, CurrentWVtx);
					CachedScores.Add(OtherVtxIndex, Score);

					if (const int32 TargetIndex = Heuristics->GetQueueingIndex(OpenList, Score); TargetIndex == -1) { OpenList.Add(NewWVtx); }
					else { OpenList.Insert(NewWVtx, TargetIndex); }
				}
			}
		}

		PCGEX_DELETE_TARRAY(ClosedList)
		PCGEX_DELETE_TARRAY(OpenList)

		return bSuccess;
	}

	static bool FindPath(
		const PCGExMesh::FMesh* Mesh, const FVector& SeedPosition, const FVector& GoalPosition,
		const UPCGExHeuristicOperation* Heuristics,
		TArray<int32>& OutPath)
	{
		return FindPath(
			Mesh,
			Mesh->FindClosestVertex(SeedPosition),
			Mesh->FindClosestVertex(GoalPosition),
			Heuristics, OutPath);
	}

	static bool ContinuePath(
		const PCGExMesh::FMesh* Mesh, const int32 To,
		const UPCGExHeuristicOperation* Heuristics,
		TArray<int32>& OutPath)
	{
		return FindPath(Mesh, OutPath.Last(), To, Heuristics, OutPath);
	}

	static bool ContinuePath(
		const PCGExMesh::FMesh* Mesh, const FVector& To,
		const UPCGExHeuristicOperation* Heuristics,
		TArray<int32>& OutPath)
	{
		return ContinuePath(Mesh, Mesh->FindClosestVertex(To), Heuristics, OutPath);
	}
}

// Define the background task class
class PCGEXTENDEDTOOLKIT_API FPCGExPathfindingTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExPathfindingTask(
		FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		PCGExPathfinding::FPathQuery* InQuery) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		Query(InQuery)
	{
	}

	PCGExPathfinding::FPathQuery* Query = nullptr;
};
