// Copyright Timothé Lapetite 2023
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
	HigherIsBetter UMETA(DisplayName = "Higher is Better", Tooltip="Higher values are considered more desirable."),
	LowerIsBetter UMETA(DisplayName = "Lower is Better", Tooltip="Lower values are considered more desirable."),
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1))
	double Weight = 100;

	/** Fetch weight from attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle, DisplayPriority=1))
	bool bUseLocalWeight = false;

	/** Attribute to fetch local weight from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseLocalWeight", DisplayPriority=1))
	FPCGExInputDescriptorWithSingleField LocalWeight;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExHeuristicModifiersSettings
{
	GENERATED_BODY()

	/** Draw line thickness. */
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

	void PrepareForData(PCGExData::FPointIO& InPoints, PCGExData::FPointIO& InEdges, const double Scale = 1)
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

		for (const FPCGExHeuristicModifier& Modifier : Modifiers)
		{
			if (!Modifier.bEnabled) { continue; }

			PCGEx::FLocalSingleFieldGetter* NewGetter = new PCGEx::FLocalSingleFieldGetter();
			NewGetter->Capture(Modifier);

			PCGEx::FLocalSingleFieldGetter* WeightGetter = nullptr;

			if (Modifier.bUseLocalWeight)
			{
				WeightGetter = new PCGEx::FLocalSingleFieldGetter();
				WeightGetter->Capture(Modifier.LocalWeight);
			}

			bool bSuccess;
			bool bLocalWeight = false;
			if (Modifier.Source == EPCGExHeuristicScoreSource::Point)
			{
				if (!bUpdatePoints) { continue; }
				bSuccess = NewGetter->Bind(InPoints);
				if (WeightGetter) { bLocalWeight = WeightGetter->Bind(InPoints); }
				TargetArray = &PointScoreModifiers;
				NumIterations = NumPoints;
			}
			else
			{
				bSuccess = NewGetter->Bind(InEdges);
				if (WeightGetter) { bLocalWeight = WeightGetter->Bind(InEdges); }
				TargetArray = &EdgeScoreModifiers;
				NumIterations = NumEdges;
			}

			if (!bSuccess || !NewGetter->bValid || !NewGetter->bEnabled)
			{
				PCGEX_DELETE(NewGetter)
				continue;
			}

			double MinValue = TNumericLimits<double>::Max();
			double MaxValue = TNumericLimits<double>::Min();
			for (int i = 0; i < NumIterations; i++)
			{
				const double Value = NewGetter->Values[i];
				MinValue = FMath::Min(MinValue, Value);
				MaxValue = FMath::Max(MaxValue, Value);
			}

			double OutMin = -1;
			double OutMax = 1;
			const double OutScale = Scale;

			if (Modifier.Interpretation == EPCGExHeuristicScoreMode::HigherIsBetter)
			{
				OutMin = 1;
				OutMax = -1;
			}
			
			if (bLocalWeight)
			{
				for (int i = 0; i < NumIterations; i++)
				{
					(*TargetArray)[i] += (PCGExMath::Remap(NewGetter->Values[i], MinValue, MaxValue, OutMin, OutMax) * WeightGetter->Values[i]) * OutScale;
				}
			}
			else
			{
				for (int i = 0; i < NumIterations; i++)
				{
					(*TargetArray)[i] += (PCGExMath::Remap(NewGetter->Values[i], MinValue, MaxValue, OutMin, OutMax) * Modifier.Weight) * OutScale;
				}
			}

			PCGEX_DELETE(NewGetter)
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

	static bool FindPath(
		const PCGExCluster::FCluster* Cluster,
		const int32 Seed, const int32 Goal,
		const UPCGExHeuristicOperation* Heuristics,
		const FPCGExHeuristicModifiersSettings* Modifiers, TArray<int32>& OutPath)
	{
		if (Seed == Goal) { return false; }

		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathfinding::FindPath);

		const PCGExCluster::FVertex& StartVtx = Cluster->Vertices[Seed];
		const PCGExCluster::FVertex& EndVtx = Cluster->Vertices[Goal];

		// Basic A* implementation
		TMap<int32, double> CachedScores;

		TArray<PCGExCluster::FScoredVertex*> OpenList;
		OpenList.Reserve(Cluster->Vertices.Num() / 3);

		TArray<PCGExCluster::FScoredVertex*> ClosedList;
		ClosedList.Reserve(Cluster->Vertices.Num() / 3);
		TSet<int32> Visited;

		OpenList.Add(new PCGExCluster::FScoredVertex(StartVtx, 0));
		bool bSuccess = false;

		while (!bSuccess && !OpenList.IsEmpty())
		{
			PCGExCluster::FScoredVertex* CurrentWVtx = OpenList.Pop();
			const int32 CurrentVtxIndex = CurrentWVtx->Vertex->ClusterIndex;

			ClosedList.Add(CurrentWVtx);
			Visited.Add(CurrentWVtx->Vertex->PointIndex);

			if (CurrentVtxIndex == EndVtx.ClusterIndex)
			{
				bSuccess = true;
				TArray<int32> Path;

				while (CurrentWVtx)
				{
					Path.Add(CurrentWVtx->Vertex->ClusterIndex);
					CurrentWVtx = CurrentWVtx->From;
				}

				Algo::Reverse(Path);
				OutPath.Append(Path);
			}
			else
			{
				//Get current index neighbors
				for (const PCGExCluster::FVertex& Vtx = Cluster->GetVertex(CurrentVtxIndex);
				     const int32 EdgeIndex : Vtx.Edges) //TODO: Use edge instead?
				{
					const PCGExGraph::FIndexedEdge& Edge = Cluster->Edges[EdgeIndex];
					const int32 OtherPointIndex = Edge.Other(Vtx.PointIndex);
					if (Visited.Contains(OtherPointIndex)) { continue; }

					const PCGExCluster::FVertex& OtherVtx = Cluster->GetVertexFromPointIndex(OtherPointIndex);
					double Score = Heuristics->ComputeScore(CurrentWVtx, OtherVtx, StartVtx, EndVtx, Edge);
					Score += Modifiers->GetScore(OtherVtx.PointIndex, Edge.Index);

					if (const double* PreviousScore = CachedScores.Find(OtherPointIndex);
						PreviousScore && !Heuristics->IsBetterScore(*PreviousScore, Score))
					{
						continue;
					}

					PCGExCluster::FScoredVertex* NewWVtx = new PCGExCluster::FScoredVertex(OtherVtx, Score, CurrentWVtx);
					CachedScores.Add(OtherPointIndex, Score);

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
		const PCGExCluster::FCluster* Cluster, const FVector& SeedPosition, const FVector& GoalPosition,
		const UPCGExHeuristicOperation* Heuristics,
		const FPCGExHeuristicModifiersSettings* Modifiers, TArray<int32>& OutPath)
	{
		return FindPath(
			Cluster,
			Cluster->FindClosestVertex(SeedPosition),
			Cluster->FindClosestVertex(GoalPosition),
			Heuristics, Modifiers, OutPath);
	}


	static bool ContinuePath(
		const PCGExCluster::FCluster* Cluster, const int32 To,
		const UPCGExHeuristicOperation* Heuristics,
		const FPCGExHeuristicModifiersSettings* Modifiers, TArray<int32>& OutPath)
	{
		return FindPath(Cluster, OutPath.Last(), To, Heuristics, Modifiers, OutPath);
	}


	static bool ContinuePath(
		const PCGExCluster::FCluster* Cluster, const FVector& To,
		const UPCGExHeuristicOperation* Heuristics,
		const FPCGExHeuristicModifiersSettings* Modifiers, TArray<int32>& OutPath)
	{
		return ContinuePath(Cluster, Cluster->FindClosestVertex(To), Heuristics, Modifiers, OutPath);
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
