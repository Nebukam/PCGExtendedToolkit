// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExPointElements.h"
#include "PCGExPathfinding.generated.h"

namespace PCGEx
{
	class FScoredQueue;
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
struct PCGEXELEMENTSPATHFINDING_API FPCGExPathStatistics
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
	namespace Labels
	{
		const FName SourceOverridesGoalPicker = TEXT("Overrides : Goal Picker");
		const FName SourceOverridesSearch = TEXT("Overrides : Search");
	}

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

	struct PCGEXELEMENTSPATHFINDING_API FNodePick
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

	struct PCGEXELEMENTSPATHFINDING_API FSeedGoalPair
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

	PCGEXELEMENTSPATHFINDING_API
	void ProcessGoals(const TSharedPtr<PCGExData::FFacade>& InSeedDataFacade, const UPCGExGoalPicker* GoalPicker, TFunction<void(int32, int32)>&& GoalFunc);
}
