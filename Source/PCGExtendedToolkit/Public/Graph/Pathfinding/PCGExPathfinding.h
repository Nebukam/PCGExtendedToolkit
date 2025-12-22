// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExPointElements.h"
#include "PCGExPathfinding.generated.h"

namespace PCGEx
{
	class FHashLookup;
}

namespace PCGExMT
{
	class FTaskManager;
}

class UPCGExGoalPicker;

namespace PCGExData
{
	class FFacade;
}

namespace PCGExSearch
{
	class FScoredQueue;
}

struct FPCGExNodeSelectionDetails;

namespace PCGExClusters
{
	class FCluster;
	struct FNode;
}

namespace PCGExHeuristics
{
	class FLocalFeedbackHandler;
}

class FPCGExSearchOperation;

namespace PCGExHeuristics
{
	class FHandler;
}

UENUM()
enum class EPCGExPathComposition : uint8
{
	Vtx         = 0 UMETA(DisplayName = "Vtx", Tooltip="..."),
	Edges       = 1 UMETA(DisplayName = "Edge", Tooltip="..."),
	VtxAndEdges = 2 UMETA(Hidden, DisplayName = "Vtx & Edges", Tooltip="..."),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExPathStatistics
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

	struct PCGEXTENDEDTOOLKIT_API FNodePick
	{
		explicit FNodePick(const PCGExData::FConstPoint& InSourcePointRef)
			: Point(InSourcePointRef)
		{
		}

		PCGExData::FConstPoint Point;
		const PCGExClusters::FNode* Node = nullptr;

		bool IsValid() const { return Node != nullptr; };
		bool ResolveNode(const TSharedRef<PCGExClusters::FCluster>& InCluster, const FPCGExNodeSelectionDetails& SelectionDetails);

		operator PCGExData::FConstPoint() const { return Point; }
	};

	struct PCGEXTENDEDTOOLKIT_API FSeedGoalPair
	{
		int32 Seed = -1;
		FVector SeedPosition = FVector::ZeroVector;
		int32 Goal = -1;
		FVector GoalPosition = FVector::ZeroVector;

		FSeedGoalPair() = default;

		FSeedGoalPair(const int32 InSeed, const FVector& InSeedPosition, const int32 InGoal, const FVector& InGoalPosition)
			: Seed(InSeed), SeedPosition(InSeedPosition), Goal(InGoal), GoalPosition(InGoalPosition)
		{
		}

		FSeedGoalPair(const PCGExData::FConstPoint& InSeed, const PCGExData::FConstPoint& InGoal)
			: Seed(InSeed.Index), SeedPosition(InSeed.GetLocation()), Goal(InGoal.Index), GoalPosition(InGoal.GetLocation())
		{
		}

		bool IsValid() const { return Seed != -1 && Goal != -1; }
	};

	class PCGEXTENDEDTOOLKIT_API FSearchAllocations : public TSharedFromThis<FSearchAllocations>
	{
	protected:
		int32 NumNodes = 0;

	public:
		FSearchAllocations() = default;

		TBitArray<> Visited;
		TArray<double> GScore;
		TSharedPtr<PCGEx::FHashLookup> TravelStack;
		TSharedPtr<PCGExSearch::FScoredQueue> ScoredQueue;

		void Init(const PCGExClusters::FCluster* InCluster);
		void Reset();
	};

	class PCGEXTENDEDTOOLKIT_API FPathQuery : public TSharedFromThis<FPathQuery>
	{
	public:
		FPathQuery(const TSharedRef<PCGExClusters::FCluster>& InCluster, const FNodePick& InSeed, const FNodePick& InGoal, const int32 InQueryIndex)
			: Cluster(InCluster), Seed(InSeed), Goal(InGoal), QueryIndex(InQueryIndex)
		{
		}

		FPathQuery(const TSharedRef<PCGExClusters::FCluster>& InCluster, const PCGExData::FConstPoint& InSeed, const PCGExData::FConstPoint& InGoal, const int32 InQueryIndex)
			: Cluster(InCluster), Seed(InSeed), Goal(InGoal), QueryIndex(InQueryIndex)
		{
		}

		FPathQuery(const TSharedRef<PCGExClusters::FCluster>& InCluster, const TSharedPtr<FPathQuery>& PreviousQuery, const PCGExData::FConstPoint& InGoalPointRef, const int32 InQueryIndex)
			: Cluster(InCluster), Seed(PreviousQuery->Goal), Goal(InGoalPointRef), QueryIndex(InQueryIndex)
		{
		}

		FPathQuery(const TSharedRef<PCGExClusters::FCluster>& InCluster, const TSharedPtr<FPathQuery>& PreviousQuery, const TSharedPtr<FPathQuery>& NextQuery, const int32 InQueryIndex)
			: Cluster(InCluster), Seed(PreviousQuery->Goal), Goal(NextQuery->Seed), QueryIndex(InQueryIndex)
		{
		}

		TSharedRef<PCGExClusters::FCluster> Cluster;

		FNodePick Seed;
		FNodePick Goal;
		EQueryPickResolution PickResolution = EQueryPickResolution::None;

		TArray<int32> PathNodes;
		TArray<int32> PathEdges;
		EPathfindingResolution Resolution = EPathfindingResolution::None;

		const int32 QueryIndex = -1;

		bool HasValidEndpoints() const { return Seed.IsValid() && Goal.IsValid() && PickResolution == EQueryPickResolution::Success; };
		bool HasValidPathPoints() const { return PathNodes.Num() >= 2; };
		bool IsQuerySuccessful() const { return Resolution == EPathfindingResolution::Success; };

		EQueryPickResolution ResolvePicks(const FPCGExNodeSelectionDetails& SeedSelectionDetails, const FPCGExNodeSelectionDetails& GoalSelectionDetails);

		void Reserve(const int32 NumReserve);
		void AddPathNode(const int32 InNodeIndex, const int32 InEdgeIndex = -1);
		void SetResolution(const EPathfindingResolution InResolution);

		void FindPath(const TSharedPtr<FPCGExSearchOperation>& SearchOperation, const TSharedPtr<FSearchAllocations>& Allocations, const TSharedPtr<PCGExHeuristics::FHandler>& HeuristicsHandler, const TSharedPtr<PCGExHeuristics::FLocalFeedbackHandler>& LocalFeedback);

		void AppendNodePoints(TArray<int32>& OutPoints, const int32 TruncateStart = 0, const int32 TruncateEnd = 0) const;

		void AppendEdgePoints(TArray<int32>& OutPoints) const;

		void Cleanup();
	};

	class PCGEXTENDEDTOOLKIT_API FPlotQuery : public TSharedFromThis<FPlotQuery>
	{
		TSharedPtr<PCGExHeuristics::FLocalFeedbackHandler> LocalFeedbackHandler;

	public:
		explicit FPlotQuery(const TSharedRef<PCGExClusters::FCluster>& InCluster, bool ClosedLoop, const int32 InQueryIndex)
			: Cluster(InCluster), bIsClosedLoop(ClosedLoop), QueryIndex(InQueryIndex)
		{
		}

		TSharedRef<PCGExClusters::FCluster> Cluster;
		bool bIsClosedLoop = false;
		TSharedPtr<PCGExData::FFacade> PlotFacade;
		const int32 QueryIndex = -1;

		TArray<TSharedPtr<FPathQuery>> SubQueries;

		using CompletionCallback = std::function<void(const TSharedPtr<FPlotQuery>&)>;
		CompletionCallback OnCompleteCallback;

		void BuildPlotQuery(const TSharedPtr<PCGExData::FFacade>& InPlot, const FPCGExNodeSelectionDetails& SeedSelectionDetails, const FPCGExNodeSelectionDetails& GoalSelectionDetails);

		void FindPaths(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager, const TSharedPtr<FPCGExSearchOperation>& SearchOperation, const TSharedPtr<FSearchAllocations>& Allocations, const TSharedPtr<PCGExHeuristics::FHandler>& HeuristicsHandler);

		void Cleanup();
	};

	void ProcessGoals(const TSharedPtr<PCGExData::FFacade>& InSeedDataFacade, const UPCGExGoalPicker* GoalPicker, TFunction<void(int32, int32)>&& GoalFunc);
}
