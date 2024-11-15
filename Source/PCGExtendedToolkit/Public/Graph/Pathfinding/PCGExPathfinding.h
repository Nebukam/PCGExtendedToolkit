// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMT.h"
#include "GoalPickers/PCGExGoalPicker.h"
#include "Graph/PCGExCluster.h"

#include "PCGExPathfinding.generated.h"

namespace PCGExHeuristics
{
	class FLocalFeedbackHandler;
}

class UPCGExSearchOperation;

namespace PCGExHeuristics
{
	class FHeuristicsHandler;
}

UENUM()
enum class EPCGExPathComposition : uint8
{
	Vtx         = 0 UMETA(DisplayName = "Vtx", Tooltip="..."),
	Edges       = 1 UMETA(DisplayName = "Edge", Tooltip="..."),
	VtxAndEdges = 2 UMETA(Hidden, DisplayName = "Vtx & Edges", Tooltip="..."),
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathStatistics
{
	GENERATED_BODY()

	FPCGExPathStatistics()
	{
	}

	virtual ~FPCGExPathStatistics() = default;

	/** Write the point use count. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWritePointUseCount = false;

	/** Name of the attribute to write point use count to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayName="PointUseCount", PCG_Overridable, EditCondition="bWritePointUseCount"))
	FName PointUseCountAttributeName = FName("PointUseCount");

	/** Write the edge use count. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteEdgeUseCount = false;

	/** Name of the attribute to write edge use count to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayName="EdgeUseCount", PCG_Overridable, EditCondition="bWriteEdgeUseCount"))
	FName EdgeUseCountAttributeName = FName("EdgeUseCount");
};

namespace PCGExPathfinding
{
	const FName SourceOverridesGoalPicker = TEXT("Overrides : Goal Picker");
	const FName SourceOverridesSearch = TEXT("Overrides : Search");

	enum class EQueryPickResolution : uint8
	{
		None = 0,
		Success,
		UnresolvedSeed,
		UnresolvedGoal,
		UnresolvedPicks,
		SameSeedAndGoal,
	};

	enum class EPathfindingResolution : uint8
	{
		None = 0,
		Success,
		Fail
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FNodePick
	{
		FNodePick(const int32 InSourceIndex, const FVector& InSourcePosition):
			SourceIndex(InSourceIndex), SourcePosition(InSourcePosition)
		{
		}

		explicit FNodePick(const PCGExData::FPointRef& InSourcePointRef):
			SourceIndex(InSourcePointRef.Index), SourcePosition(InSourcePointRef.Point->Transform.GetLocation())
		{
		}

		int32 SourceIndex = -1;
		FVector SourcePosition = FVector::ZeroVector;
		const PCGExCluster::FNode* Node = nullptr;

		bool IsValid() const { return Node != nullptr; };
		bool ResolveNode(const TSharedRef<PCGExCluster::FCluster>& InCluster, const FPCGExNodeSelectionDetails& SelectionDetails);
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FSeedGoalPair
	{
		int32 Seed = -1;
		FVector SeedPosition = FVector::ZeroVector;
		int32 Goal = -1;
		FVector GoalPosition = FVector::ZeroVector;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FPathQuery : public TSharedFromThis<FPathQuery>
	{
	public:
		FPathQuery(
			const TSharedRef<PCGExCluster::FCluster>& InCluster,
			const PCGExData::FPointRef& InSeedPointRef,
			const PCGExData::FPointRef& InGoalPointRef)
			: Cluster(InCluster), Seed(InSeedPointRef), Goal(InGoalPointRef)
		{
		}

		FPathQuery(
			const TSharedRef<PCGExCluster::FCluster>& InCluster,
			const TSharedPtr<FPathQuery>& PreviousQuery,
			const PCGExData::FPointRef& InGoalPointRef)
			: Cluster(InCluster), Seed(PreviousQuery->Goal), Goal(InGoalPointRef)
		{
		}

		FPathQuery(
			const TSharedRef<PCGExCluster::FCluster>& InCluster,
			const TSharedPtr<FPathQuery>& PreviousQuery,
			const TSharedPtr<FPathQuery>& NextQuery)
			: Cluster(InCluster), Seed(PreviousQuery->Goal), Goal(NextQuery->Seed)
		{
		}

		TSharedRef<PCGExCluster::FCluster> Cluster;

		FNodePick Seed;
		FNodePick Goal;
		EQueryPickResolution PickResolution = EQueryPickResolution::None;

		TArray<int32> PathNodes;
		TArray<int32> PathEdges;
		EPathfindingResolution Resolution = EPathfindingResolution::None;

		bool HasValidEndpoints() const { return Seed.IsValid() && Goal.IsValid() && PickResolution == EQueryPickResolution::Success; };
		bool HasValidPathPoints() const { return PathNodes.Num() >= 2; };
		bool IsQuerySuccessful() const { return Resolution == EPathfindingResolution::Success; };

		EQueryPickResolution ResolvePicks(
			const FPCGExNodeSelectionDetails& SeedSelectionDetails,
			const FPCGExNodeSelectionDetails& GoalSelectionDetails);

		void Reserve(const int32 NumReserve);
		void AddPathNode(const int32 InNodeIndex, const int32 InEdgeIndex = -1);
		void SetResolution(const EPathfindingResolution InResolution);

		void FindPath(
			const UPCGExSearchOperation* SearchOperation,
			const TSharedPtr<PCGExHeuristics::FHeuristicsHandler>& HeuristicsHandler,
			const TSharedPtr<PCGExHeuristics::FLocalFeedbackHandler>& LocalFeedback);

		void AppendNodePoints(
			TArray<FPCGPoint>& OutPoints,
			const int32 TruncateStart = 0,
			const int32 TruncateEnd = 0) const;

		void AppendEdgePoints(TArray<FPCGPoint>& OutPoints) const;

		void Cleanup();
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FPlotQuery : public TSharedFromThis<FPlotQuery>
	{
		TSharedPtr<PCGExHeuristics::FLocalFeedbackHandler> LocalFeedbackHandler;

	public:
		explicit FPlotQuery(const TSharedRef<PCGExCluster::FCluster>& InCluster, bool ClosedLoop = false)
			: Cluster(InCluster), bIsClosedLoop(ClosedLoop)
		{
		}

		TSharedRef<PCGExCluster::FCluster> Cluster;
		bool bIsClosedLoop = false;
		TSharedPtr<PCGExData::FFacade> PlotFacade;

		TArray<TSharedPtr<FPathQuery>> SubQueries;

		using CompletionCallback = std::function<void(const TSharedPtr<FPlotQuery>&)>;
		CompletionCallback OnCompleteCallback;

		void BuildPlotQuery(
			const TSharedPtr<PCGExData::FFacade>& InPlot,
			const FPCGExNodeSelectionDetails& SeedSelectionDetails,
			const FPCGExNodeSelectionDetails& GoalSelectionDetails);

		void FindPaths(
			const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager,
			const UPCGExSearchOperation* SearchOperation,
			const TSharedPtr<PCGExHeuristics::FHeuristicsHandler>& HeuristicsHandler);

		void Cleanup();
	};

	static void ProcessGoals(
		const TSharedPtr<PCGExData::FFacade>& InSeedDataFacade,
		const UPCGExGoalPicker* GoalPicker,
		TFunction<void(int32, int32)>&& GoalFunc)
	{
		for (int PointIndex = 0; PointIndex < InSeedDataFacade->Source->GetNum(); PointIndex++)
		{
			const PCGExData::FPointRef& Seed = InSeedDataFacade->Source->GetInPointRef(PointIndex);

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

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathfindingTask : public PCGExMT::FPCGExTask
{
public:
	FPCGExPathfindingTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO,
	                      const TArray<PCGExPathfinding::FSeedGoalPair>* InQueries) :
		FPCGExTask(InPointIO),
		Queries(InQueries)
	{
	}

	const TArray<PCGExPathfinding::FSeedGoalPair>* Queries = nullptr;
};
