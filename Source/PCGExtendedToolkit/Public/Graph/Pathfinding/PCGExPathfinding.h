// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMT.h"
#include "PCGExPointsProcessor.h"
#include "GoalPickers/PCGExGoalPicker.h"
#include "Graph/PCGExCluster.h"
#include "Heuristics/PCGExHeuristicOperation.h"

#include "PCGExPathfinding.generated.h"

UENUM(BlueprintType)
enum class EPCGExPathfindingNavmeshMode : uint8
{
	Regular UMETA(DisplayName = "Regular", ToolTip="Regular pathfinding"),
	Hierarchical UMETA(DisplayName = "HIerarchical", ToolTip="Cell-based pathfinding"),
};

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


UENUM(BlueprintType)
enum class EPCGExHeuristicScoreMode : uint8
{
	LowerIsBetter UMETA(DisplayName = "Lower is Better", Tooltip="Lower values are considered more desirable."),
	HigherIsBetter UMETA(DisplayName = "Higher is Better", Tooltip="Higher values are considered more desirable."),
};

UENUM(BlueprintType)
enum class EPCGExHeuristicScoreSource : uint8
{
	Point UMETA(DisplayName = "Point", Tooltip="Value is fetched from the point being evaluated."),
	Edge UMETA(DisplayName = "Edge", Tooltip="Value is fetched from the edge connecting to the point being evaluated."),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExHeuristicModifier : public FPCGExInputDescriptorWithSingleField
{
	GENERATED_BODY()

	FPCGExHeuristicModifier()
	{
	}

	virtual ~FPCGExHeuristicModifier() override
	{
	}

	/** Enable or disable this modifier. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-4))
	bool bEnabled = true;

	/** Read the data from either vertices or edges */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-3))
	EPCGExHeuristicScoreSource Source = EPCGExHeuristicScoreSource::Point;

	/** How to interpret the data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-2))
	EPCGExHeuristicScoreMode Interpretation = EPCGExHeuristicScoreMode::HigherIsBetter;

	/** Modifier weight. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1, ClampMin="1"))
	double Weight = 100;

	/** Fetch weight from attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle, DisplayPriority=1))
	bool bUseLocalWeight = false;

	/** Attribute to fetch local weight from. This value will be scaled by the base weight. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseLocalWeight", DisplayPriority=1))
	FPCGExInputDescriptorWithSingleField LocalWeight;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExHeuristicModifiersSettings
{
	GENERATED_BODY()

	/** Reference weight. This is used by unexposed heuristics computations to stay consistent with user-defined modifiers.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double ReferenceWeight = 100;

	double TotalWeight = 0;
	double Scale = 0;

	/** List of weighted heuristic modifiers */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, TitleProperty="{TitlePropertyName} ({Interpretation})"))
	TArray<FPCGExHeuristicModifier> Modifiers;

	PCGExData::FPointIO* LastPoints = nullptr;
	TArray<double> PointScoreModifiers;
	TArray<double> EdgeScoreModifiers;

	FPCGExHeuristicModifiersSettings()
	{
		PointScoreModifiers.Empty();
		EdgeScoreModifiers.Empty();
	}

	~FPCGExHeuristicModifiersSettings()
	{
		Cleanup();
	}

	void Cleanup()
	{
		LastPoints = nullptr;
		PointScoreModifiers.Empty();
		EdgeScoreModifiers.Empty();
	}

#if WITH_EDITOR
	void UpdateUserFacingInfos()
	{
		for (FPCGExHeuristicModifier& Modifier : Modifiers) { Modifier.UpdateUserFacingInfos(); }
	}
#endif

	void PrepareForData(PCGExData::FPointIO& InPoints, PCGExData::FPointIO& InEdges)
	{
		bool bUpdatePoints = false;
		const int32 NumPoints = InPoints.GetNum();
		const int32 NumEdges = InEdges.GetNum();

		if (LastPoints != &InPoints)
		{
			LastPoints = &InPoints;
			bUpdatePoints = true;

			InPoints.CreateInKeys();
			PointScoreModifiers.SetNumZeroed(NumPoints);
		}

		InEdges.CreateInKeys();
		EdgeScoreModifiers.SetNumZeroed(NumEdges);

		TArray<double>* TargetArray;
		int32 NumIterations;

		TotalWeight = ReferenceWeight;
		for (const FPCGExHeuristicModifier& Modifier : Modifiers)
		{
			if (!Modifier.bEnabled) { continue; }
			TotalWeight += FMath::Abs(Modifier.Weight);
		}

		Scale = ReferenceWeight / TotalWeight;

		for (const FPCGExHeuristicModifier& Modifier : Modifiers)
		{
			if (!Modifier.bEnabled) { continue; }

			PCGEx::FLocalSingleFieldGetter* ModifierGetter = new PCGEx::FLocalSingleFieldGetter();
			ModifierGetter->Capture(Modifier);

			PCGEx::FLocalSingleFieldGetter* WeightGetter = nullptr;

			if (Modifier.bUseLocalWeight)
			{
				WeightGetter = new PCGEx::FLocalSingleFieldGetter();
				WeightGetter->Capture(Modifier.LocalWeight);
			}

			bool bModifierGrabbed;
			bool bLocalWeightGrabbed = false;
			if (Modifier.Source == EPCGExHeuristicScoreSource::Point)
			{
				if (!bUpdatePoints) { continue; }
				bModifierGrabbed = ModifierGetter->Grab(InPoints, true);
				if (WeightGetter) { bLocalWeightGrabbed = WeightGetter->Grab(InPoints); }
				TargetArray = &PointScoreModifiers;
				NumIterations = NumPoints;
			}
			else
			{
				bModifierGrabbed = ModifierGetter->Grab(InEdges, true);
				if (WeightGetter) { bLocalWeightGrabbed = WeightGetter->Grab(InEdges); }
				TargetArray = &EdgeScoreModifiers;
				NumIterations = NumEdges;
			}

			if (!bModifierGrabbed || !ModifierGetter->bValid || !ModifierGetter->bEnabled)
			{
				PCGEX_DELETE(ModifierGetter)
				continue;
			}

			const double MinValue = ModifierGetter->Min;
			const double MaxValue = ModifierGetter->Max;
			const double WeightScale = Modifier.Weight / TotalWeight;

			// Default is, lower score is better.
			double OutMin = 0;
			double OutMax = 1;

			if (Modifier.Interpretation == EPCGExHeuristicScoreMode::HigherIsBetter)
			{
				// Make the score substractive
				OutMin = 1;
				OutMax = 0;
			}

			if (bLocalWeightGrabbed)
			{
				for (int i = 0; i < NumIterations; i++)
				{
					(*TargetArray)[i] += (PCGExMath::Remap(ModifierGetter->Values[i], MinValue, MaxValue, OutMin, OutMax) * (WeightGetter->Values[i] / TotalWeight));
				}
			}
			else
			{
				for (int i = 0; i < NumIterations; i++)
				{
					(*TargetArray)[i] += (PCGExMath::Remap(ModifierGetter->Values[i], MinValue, MaxValue, OutMin, OutMax) * WeightScale);
				}
			}

			PCGEX_DELETE(ModifierGetter)
			PCGEX_DELETE(WeightGetter)
		}
	}

	double GetScore(const int32 PointIndex, const int32 EdgeIndex) const
	{
		return PointScoreModifiers[PointIndex] + EdgeScoreModifiers[EdgeIndex];
	}
};


namespace PCGExPathfinding
{
	struct PCGEXTENDEDTOOLKIT_API FPlotPoint
	{
		int32 PlotIndex;
		FVector Position;
		PCGMetadataEntryKey MetadataEntryKey = -1;
	};


	const FName SourceSeedsLabel = TEXT("Seeds");
	const FName SourceGoalsLabel = TEXT("Goals");
	const FName SourcePlotsLabel = TEXT("Plots");

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

	/** Find the shortest path but stops as soon as it reaches its goal*/
	static bool FindPath_AStar(
		const PCGExCluster::FCluster* Cluster,
		const int32 Seed, const int32 Goal,
		const UPCGExHeuristicOperation* Heuristics,
		const FPCGExHeuristicModifiersSettings* Modifiers, TArray<int32>& OutPath)
	{
		if (Seed == Goal) { return false; }

		const int32 NumNodes = Cluster->Nodes.Num();

		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathfinding::FindPath);

		const PCGExCluster::FNode& SeedNode = Cluster->Nodes[Seed];
		const PCGExCluster::FNode& GoalNode = Cluster->Nodes[Goal];

		// Basic A* implementation

		// For node n, cameFrom[n] is the node immediately preceding it on the cheapest path from the start
		// to n currently known.
		TMap<int32, int32> CameFrom;
		TSet<int32> Visited;
		Visited.Reserve(NumNodes);

		// For node n, gScore[n] is the cost of the cheapest path from start to n currently known.
		TArray<double> GScore;

		// For node n, fScore[n] := gScore[n] + h(n). fScore[n] represents our current best guess as to
		// how cheap a path could be from start to finish if it goes through n.
		TArray<double> FScore;

		GScore.SetNum(NumNodes);
		FScore.SetNum(NumNodes);

		for (int i = 0; i < NumNodes; i++)
		{
			GScore[i] = TNumericLimits<double>::Max();
			FScore[i] = TNumericLimits<double>::Max();
		}

		GScore[SeedNode.NodeIndex] = 0;
		FScore[SeedNode.NodeIndex] = Heuristics->ComputeFScore(SeedNode, SeedNode, GoalNode);

		TArray<int32> OpenList;
		OpenList.Add(SeedNode.NodeIndex);

		bool bSuccess = false;

		while (!OpenList.IsEmpty())
		{
			// the node in openSet having the lowest FScore value
			const PCGExCluster::FNode& Current = Cluster->Nodes[OpenList.Pop(false)]; // TODO: Sorted add, otherwise this won't work.
					Visited.Remove(Current.NodeIndex);

			if (Current.NodeIndex == GoalNode.NodeIndex)
			{
				bSuccess = true;
				TArray<int32> Path;
				int32 PathIndex = Current.NodeIndex;

				//WARNING: This will loop forever if we introduce negative weights.

				while (PathIndex != -1)
				{
					Path.Add(PathIndex);
					const int32* PathIndexPtr = CameFrom.Find(PathIndex);
					PathIndex = PathIndexPtr ? *PathIndexPtr : -1;
				}

				Algo::Reverse(Path);
				OutPath.Append(Path);

				break;
			}

			const double CurrentGScore = GScore[Current.NodeIndex];

			//Get current index neighbors
			for (const int32 AdjacentNodeIndex : Current.AdjacentNodes)
			{
				const PCGExCluster::FNode& AdjacentNode = Cluster->Nodes[AdjacentNodeIndex];
				const PCGExGraph::FIndexedEdge& Edge = Cluster->GetEdgeFromNodeIndices(Current.NodeIndex, AdjacentNodeIndex);

				// d(current,neighbor) is the weight of the edge from current to neighbor
				// tentative_gScore is the distance from start to the neighbor through current
				const double ScoreOffset = Modifiers->GetScore(AdjacentNode.PointIndex, Edge.EdgeIndex);
				const double TentativeGScore = CurrentGScore + Heuristics->ComputeDScore(Current, AdjacentNode, Edge, SeedNode, GoalNode) + ScoreOffset;

				if (TentativeGScore >= GScore[AdjacentNode.NodeIndex]) { continue; }

				CameFrom.Add(AdjacentNode.NodeIndex, Current.NodeIndex);
				GScore[AdjacentNode.NodeIndex] = TentativeGScore;
				FScore[AdjacentNode.NodeIndex] = TentativeGScore + Heuristics->ComputeFScore(AdjacentNode, SeedNode, GoalNode) + ScoreOffset;

				if (!Visited.Contains(AdjacentNode.NodeIndex))
				{
					PCGEX_SORTED_ADD(OpenList, AdjacentNode.NodeIndex, TentativeGScore < GScore[i])
					Visited.Add(AdjacentNode.NodeIndex);
				}
			}
		}

		return bSuccess;
	}

	static bool FindPath_AStar(
		const PCGExCluster::FCluster* Cluster, const FVector& SeedPosition, const FVector& GoalPosition,
		const UPCGExHeuristicOperation* Heuristics,
		const FPCGExHeuristicModifiersSettings* Modifiers, TArray<int32>& OutPath)
	{
		return FindPath_AStar(
			Cluster,
			Cluster->FindClosestNode(SeedPosition),
			Cluster->FindClosestNode(GoalPosition),
			Heuristics, Modifiers, OutPath);
	}

	/** Find the best path but is super slow as it goes through mostly all possibilities*/
	static bool FindPath_Dijkstra(
		const PCGExCluster::FCluster* Cluster,
		const int32 Seed, const int32 Goal,
		const UPCGExHeuristicOperation* Heuristics,
		const FPCGExHeuristicModifiersSettings* Modifiers, TArray<int32>& OutPath)
	{
		if (Seed == Goal) { return false; }

		const int32 NumNodes = Cluster->Nodes.Num();

		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathfinding::FindPath);

		const PCGExCluster::FNode& SeedNode = Cluster->Nodes[Seed];
		const PCGExCluster::FNode& GoalNode = Cluster->Nodes[Goal];

		// Basic A* implementation

		// For node n, cameFrom[n] is the node immediately preceding it on the cheapest path from the start
		// to n currently known.
		TMap<int32, int32> CameFrom;
		TSet<int32> Visited;
		Visited.Reserve(NumNodes);

		// For node n, gScore[n] is the cost of the cheapest path from start to n currently known.
		TArray<double> GScore;

		// For node n, fScore[n] := gScore[n] + h(n). fScore[n] represents our current best guess as to
		// how cheap a path could be from start to finish if it goes through n.
		TArray<double> FScore;

		GScore.SetNum(NumNodes);
		FScore.SetNum(NumNodes);

		for (int i = 0; i < NumNodes; i++)
		{
			GScore[i] = TNumericLimits<double>::Max();
			FScore[i] = TNumericLimits<double>::Max();
		}

		GScore[SeedNode.NodeIndex] = 0;
		FScore[SeedNode.NodeIndex] = Heuristics->ComputeFScore(SeedNode, SeedNode, GoalNode);

		TArray<int32> OpenList;
		OpenList.Add(SeedNode.NodeIndex);

		bool bSuccess = false;

		while (!OpenList.IsEmpty())
		{
			// the node in openSet having the lowest FScore value
			const PCGExCluster::FNode& Current = Cluster->Nodes[OpenList.Pop(false)]; // TODO: Sorted add, otherwise this won't work.
					Visited.Remove(Current.NodeIndex);

			if (Current.NodeIndex == GoalNode.NodeIndex) { bSuccess = true; }
			const double CurrentGScore = GScore[Current.NodeIndex];

			//Get current index neighbors
			for (const int32 AdjacentNodeIndex : Current.AdjacentNodes)
			{
				const PCGExCluster::FNode& AdjacentNode = Cluster->Nodes[AdjacentNodeIndex];
				const PCGExGraph::FIndexedEdge& Edge = Cluster->GetEdgeFromNodeIndices(Current.NodeIndex, AdjacentNodeIndex);

				// d(current,neighbor) is the weight of the edge from current to neighbor
				// tentative_gScore is the distance from start to the neighbor through current
				const double ScoreOffset = Modifiers->GetScore(AdjacentNode.PointIndex, Edge.EdgeIndex);
				const double TentativeGScore = CurrentGScore + Heuristics->ComputeDScore(Current, AdjacentNode, Edge, SeedNode, GoalNode) + ScoreOffset;

				if (TentativeGScore >= GScore[AdjacentNode.NodeIndex]) { continue; }

				CameFrom.Add(AdjacentNode.NodeIndex, Current.NodeIndex);
				GScore[AdjacentNode.NodeIndex] = TentativeGScore;
				FScore[AdjacentNode.NodeIndex] = TentativeGScore + Heuristics->ComputeFScore(AdjacentNode, SeedNode, GoalNode) + ScoreOffset;

				if (!Visited.Contains(AdjacentNode.NodeIndex))
				{
					PCGEX_SORTED_ADD(OpenList, AdjacentNode.NodeIndex, TentativeGScore < GScore[i])
					Visited.Add(AdjacentNode.NodeIndex);
				}
			}
		}

		if (bSuccess)
		{
			TArray<int32> Path;
			int32 PathIndex = GoalNode.NodeIndex;

			while (PathIndex != -1)
			{
				Path.Add(PathIndex);
				const int32* PathIndexPtr = CameFrom.Find(PathIndex);
				PathIndex = PathIndexPtr ? *PathIndexPtr : -1;
			}

			Algo::Reverse(Path);
			OutPath.Append(Path);
		}

		return bSuccess;
	}

	static bool FindPath_Dijkstra(
		const PCGExCluster::FCluster* Cluster, const FVector& SeedPosition, const FVector& GoalPosition,
		const UPCGExHeuristicOperation* Heuristics,
		const FPCGExHeuristicModifiersSettings* Modifiers, TArray<int32>& OutPath)
	{
		return FindPath_Dijkstra(
			Cluster,
			Cluster->FindClosestNode(SeedPosition),
			Cluster->FindClosestNode(GoalPosition),
			Heuristics, Modifiers, OutPath);
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
