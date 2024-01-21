// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMT.h"
#include "PCGExPointsProcessor.h"
#include "GoalPickers/PCGExGoalPicker.h"
#include "Graph/PCGExCluster.h"

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
		: ScoreCurve(PCGEx::WeightDistributionLinear)
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
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-2))
	//EPCGExHeuristicScoreMode Interpretation = EPCGExHeuristicScoreMode::HigherIsBetter;

	/** Modifier weight. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1, ClampMin="0.001"))
	double Weight = 100;

	/** Fetch weight from attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable, InlineEditConditionToggle, DisplayPriority=1))
	bool bUseLocalWeight = false;

	/** Attribute to fetch local weight from. This value will be scaled by the base weight. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable, EditCondition="bUseLocalWeight", DisplayPriority=1))
	FPCGExInputDescriptorWithSingleField LocalWeight;

	/** Curve the value will be remapped over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable))
	TSoftObjectPtr<UCurveFloat> ScoreCurve;
	TObjectPtr<UCurveFloat> ScoreCurveObj;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExHeuristicModifiersSettings
{
	GENERATED_BODY()

	/** Reference weight. This is used by unexposed heuristics computations to stay consistent with user-defined modifiers.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double ReferenceWeight = 100;

	double Scale = 0;

	/** List of weighted heuristic modifiers */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, TitleProperty="{TitlePropertyName}")) // ({Interpretation})
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

	void LoadCurves()
	{
		for (FPCGExHeuristicModifier& Modifier : Modifiers)
		{
			if (!Modifier.bEnabled) { continue; }
			if (Modifier.ScoreCurve.IsNull()) { Modifier.ScoreCurveObj = TSoftObjectPtr<UCurveFloat>(PCGEx::WeightDistributionLinear).LoadSynchronous(); }
			else { Modifier.ScoreCurveObj = Modifier.ScoreCurve.LoadSynchronous(); }
		}
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

		for (const FPCGExHeuristicModifier& Modifier : Modifiers)
		{
			if (!Modifier.bEnabled) { continue; }

			PCGEx::FLocalSingleFieldGetter* ModifierGetter = new PCGEx::FLocalSingleFieldGetter();
			ModifierGetter->Capture(Modifier);

			PCGEx::FLocalSingleFieldGetter* WeightGetter = nullptr;

			const TObjectPtr<UCurveFloat> ScoreFC = Modifier.ScoreCurveObj;

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

			if (bLocalWeightGrabbed)
			{
				for (int i = 0; i < NumIterations; i++)
				{
					const double BaseValue = PCGExMath::Remap(ModifierGetter->Values[i], MinValue, MaxValue, 0, 1);
					(*TargetArray)[i] += FMath::Max(0, ScoreFC->GetFloatValue(BaseValue)) * FMath::Abs(WeightGetter->Values[i]);
				}
			}
			else
			{
				const double Factor = Modifier.Weight;
				for (int i = 0; i < NumIterations; i++)
				{
					const double BaseValue = PCGExMath::Remap(ModifierGetter->Values[i], MinValue, MaxValue, 0, 1);
					(*TargetArray)[i] += FMath::Max(0, ScoreFC->GetFloatValue(BaseValue)) * Factor;
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
}

class PCGEXTENDEDTOOLKIT_API FPCGExCompileModifiersTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExCompileModifiersTask(
		FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		PCGExData::FPointIO* InEdgeIO, FPCGExHeuristicModifiersSettings* InModifiers) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		EdgeIO(InEdgeIO), Modifiers(InModifiers)
	{
	}

	PCGExData::FPointIO* EdgeIO = nullptr;
	FPCGExHeuristicModifiersSettings* Modifiers = nullptr;

	virtual bool ExecuteTask() override
	{
		Modifiers->PrepareForData(*PointIO, *EdgeIO);
		return true;
	}
};

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
