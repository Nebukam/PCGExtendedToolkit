// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMT.h"
#include "GoalPickers/PCGExGoalPicker.h"
#include "Graph/PCGExCluster.h"

#include "PCGExPathfinding.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Pathfinding Navmesh Mode"))
enum class EPCGExPathfindingNavmeshMode : uint8
{
	Regular UMETA(DisplayName = "Regular", ToolTip="Regular pathfinding"),
	Hierarchical UMETA(DisplayName = "HIerarchical", ToolTip="Cell-based pathfinding"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Pathfinding Goal Pick Method"))
enum class EPCGExPathfindingGoalPickMethod : uint8
{
	SeedIndex UMETA(DisplayName = "Seed Index", Tooltip="Uses the seed index as goal index."),
	LocalAttribute UMETA(DisplayName = "Attribute", Tooltip="Uses a local attribute of the seed as goal index. Value is wrapped."),
	RandomPick UMETA(DisplayName = "Random Pick", Tooltip="Picks the goal randomly."),
	MultipleLocalAttribute UMETA(DisplayName = "Attribute (Multiple)", Tooltip="Uses a multiple local attribute of the seed as goal indices. Each seed will create multiple paths."),
	All UMETA(DisplayName = "All", Tooltip="Each seed will create a path for each goal."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Path Point Orientation"))
enum class EPCGExPathPointOrientation : uint8
{
	None UMETA(DisplayName = "None", Tooltip="No orientation is applied to the point"),
	Average UMETA(DisplayName = "Average", Tooltip="Orientation is averaged between previous and next point."),
	Weighted UMETA(DisplayName = "Weighted", Tooltip="Orientation is weighted based on distance."),
	WeightedInverse UMETA(DisplayName = "Weighted (Inverse)", Tooltip="Same as Weighted, but weights are swapped."),
	LookAtNext UMETA(DisplayName = "Look at Next", Tooltip="Orientation is set so the point forward axis looks at the next point"),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExPathStatistics
{
	GENERATED_BODY()

	FPCGExPathStatistics()
	{
	}

	virtual ~FPCGExPathStatistics()
	{
	}

	/** Write the point use count. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWritePointUseCount = false;

	/** Name of the attribute to write point use count to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bWritePointUseCount"))
	FName PointUseCountAttributeName = FName("UseCount");

	/** Write the edge use count. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteEdgeUseCount = false;

	/** Name of the attribute to write edge use count to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bWriteEdgeUseCount"))
	FName EdgeUseCountAttributeName = FName("UseCount");
};

namespace PCGExPathfinding
{
	constexpr PCGExMT::AsyncState State_ProcessingHeuristics = __COUNTER__;

	const FName SourceHeuristicsLabel = TEXT("Heuristics");
	const FName OutputHeuristicsLabel = TEXT("Heuristics");
	const FName OutputModifiersLabel = TEXT("Modifiers");

	struct PCGEXTENDEDTOOLKIT_API FExtraWeights //TODO: Deprecate
	{
		TArray<double> NodeExtraWeight;
		TArray<double> EdgeExtraWeight;

		double NodeScale = 1;
		double EdgeScale = 1;

		FExtraWeights(const PCGExCluster::FCluster* InCluster, const double InNodeScale, const double InEdgeScale)
			: NodeScale(InNodeScale), EdgeScale(InEdgeScale)
		{
			NodeExtraWeight.SetNumZeroed(InCluster->Nodes.Num());
			EdgeExtraWeight.SetNumZeroed(InCluster->Edges.Num());
		}

		~FExtraWeights()
		{
			NodeExtraWeight.Empty();
			EdgeExtraWeight.Empty();
		}

		void AddPointWeight(const int32 PointIndex, const double InScore)
		{
			NodeExtraWeight[PointIndex] += InScore;
		}

		void AddEdgeWeight(const int32 EdgeIndex, const double InScore)
		{
			EdgeExtraWeight[EdgeIndex] += InScore;
		}

		double GetExtraWeight(const int32 NodeIndex, const int32 EdgeIndex) const
		{
			return NodeExtraWeight[NodeIndex] + EdgeExtraWeight[EdgeIndex];
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FPlotPoint
	{
		int32 PlotIndex;
		FVector Position;
		PCGMetadataEntryKey MetadataEntryKey = -1;

		FPlotPoint(const int32 InPlotIndex, const FVector& InPosition, const PCGMetadataEntryKey InMetadataEntryKey)
			: PlotIndex(InPlotIndex), Position(InPosition), MetadataEntryKey(InMetadataEntryKey)
		{
		}
	};

	const FName SourceSeedsLabel = TEXT("Seeds");
	const FName SourceGoalsLabel = TEXT("Goals");
	const FName SourcePlotsLabel = TEXT("Plots");

	constexpr PCGExMT::AsyncState State_ProcessingHeuristicModifiers = __COUNTER__;
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

	static void ProcessGoals(
		const PCGExData::FPointIO* SeedIO,
		const UPCGExGoalPicker* GoalPicker,
		TFunction<void(int32, int32)>&& GoalFunc)
	{
		for (int PointIndex = 0; PointIndex < SeedIO->GetNum(); PointIndex++)
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
				if (GoalIndex < 0) { continue; }
				GoalFunc(PointIndex, GoalIndex);
			}
		}
	}
}

class PCGEXTENDEDTOOLKIT_API FPCGExPathfindingTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExPathfindingTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
	                      PCGExPathfinding::FPathQuery* InQuery) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		Query(InQuery)
	{
	}

	PCGExPathfinding::FPathQuery* Query = nullptr;
};
