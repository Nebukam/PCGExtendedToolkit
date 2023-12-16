// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMT.h"
#include "PCGExPointsProcessor.h"
#include "GoalPickers/PCGExGoalPicker.h"

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

	struct PCGEXTENDEDTOOLKIT_API FPathInfos
	{
		FPathInfos(const int32 InSeedIndex, const FVector& InStart, const int32 InGoalIndex, const FVector& InEnd):
			SeedIndex(InSeedIndex), StartPosition(InStart), GoalIndex(InGoalIndex), EndPosition(InEnd)
		{
		}

		int32 SeedIndex = -1;
		FVector StartPosition;
		int32 GoalIndex = -1;
		FVector EndPosition;
	};

	static bool ProcessGoals(
		FPCGExPointsProcessorContext* Context,
		const PCGExData::FPointIO* SeedIO,
		const UPCGExGoalPicker* GoalPicker,
		TFunction<void(int32, int32)>&& GoalFunc)
	{
		return Context->Process(
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
}

// Define the background task class
class PCGEXTENDEDTOOLKIT_API FPathfindingTask : public FPCGExNonAbandonableTask
{
public:
	FPathfindingTask(
		FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		PCGExPathfinding::FPathInfos* InInfos) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		Infos(InInfos)
	{
	}

	PCGExPathfinding::FPathInfos* Infos = nullptr;

};
