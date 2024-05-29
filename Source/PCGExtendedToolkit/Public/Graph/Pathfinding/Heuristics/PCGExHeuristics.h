// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCreateHeuristicsModifier.h"
#include "PCGExHeuristicFeedback.h"
#include "PCGExHeuristicsFactoryProvider.h"
#include "PCGExPointsProcessor.h"

#include "Data/PCGExGraphDefinition.h"
#include "Graph/Pathfinding/PCGExPathfinding.h"

#include "PCGExHeuristics.generated.h"

class UPCGExHeuristicFeedback;

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Heuristic Score Mode"))
enum class EPCGExHeuristicScoreMode : uint8
{
	LowerIsBetter UMETA(DisplayName = "Lower is Better", Tooltip="Lower values are considered more desirable."),
	HigherIsBetter UMETA(DisplayName = "Higher is Better", Tooltip="Higher values are considered more desirable."),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExHeuristicModifier : public FPCGExInputDescriptor
{
	GENERATED_BODY()

	FPCGExHeuristicModifier()
		: ScoreCurve(PCGEx::WeightDistributionLinear)
	{
	}

	// Transitional, will be deprecated
	explicit FPCGExHeuristicModifier(const FPCGExHeuristicModifierDescriptor& InDescriptor):
		bEnabled(true),
		Source(InDescriptor.Source),
		Weight(InDescriptor.WeightFactor),
		bUseLocalWeight(InDescriptor.bUseLocalWeightFactor),
		LocalWeightAttribute(InDescriptor.WeightFactorAttribute),
		ScoreCurve(InDescriptor.ScoreCurve)
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
	EPCGExGraphValueSource Source = EPCGExGraphValueSource::Point;

	/** Modifier weight. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1, ClampMin="0.001"))
	double Weight = 100;

	/** Fetch weight from attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable, InlineEditConditionToggle, DisplayPriority=1))
	bool bUseLocalWeight = false;

	/** Attribute to fetch local weight from. This value will be scaled by the base weight. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable, EditCondition="bUseLocalWeight", DisplayPriority=1))
	FPCGAttributePropertyInputSelector LocalWeightAttribute;

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
			PCGEX_LOAD_SOFTOBJECT(UCurveFloat, Modifier.ScoreCurve, Modifier.ScoreCurveObj, PCGEx::WeightDistributionLinear)
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
				WeightGetter->Capture(Modifier.LocalWeightAttribute);
			}

			bool bModifierGrabbed;
			bool bLocalWeightGrabbed = false;
			if (Modifier.Source == EPCGExGraphValueSource::Point)
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

namespace PCGExHeuristics
{
	struct PCGEXTENDEDTOOLKIT_API FLocalFeedbackHandler
	{
		TArray<UPCGExHeuristicFeedback*> Feedbacks;
		double TotalWeight = 0;

		FLocalFeedbackHandler()
		{
		}

		~FLocalFeedbackHandler()
		{
			for (UPCGExHeuristicFeedback* Feedback : Feedbacks)
			{
				Feedback->Cleanup();
				PCGEX_DELETE_UOBJECT(Feedback)
			}

			Feedbacks.Empty();
		}

		double GetGlobalScore(
			const PCGExCluster::FNode& From,
			const PCGExCluster::FNode& Seed,
			const PCGExCluster::FNode& Goal) const;

		double GetEdgeScore(
			const PCGExCluster::FNode& From,
			const PCGExCluster::FNode& To,
			const PCGExGraph::FIndexedEdge& Edge,
			const PCGExCluster::FNode& Seed,
			const PCGExCluster::FNode& Goal) const;

		void FeedbackPointScore(const PCGExCluster::FNode& Node);
		void FeedbackScore(const PCGExCluster::FNode& Node, const PCGExGraph::FIndexedEdge& Edge);
	};

	class PCGEXTENDEDTOOLKIT_API THeuristicsHandler
	{
	public:
		FPCGExHeuristicModifiersSettings* HeuristicsModifiers = nullptr;
		TArray<UPCGExHeuristicOperation*> Operations;
		TArray<UPCGExHeuristicFeedback*> Feedbacks;
		TArray<UPCGHeuristicsFactoryBase*> LocalFeedbackFactories;

		PCGExCluster::FCluster* CurrentCluster = nullptr;

		double ReferenceWeight = 100;
		double HeuristicsWeight = 0;
		double ModifiersWeight = 0;
		double TotalWeight = 0;

		bool HasGlobalFeedback() const { return !Feedbacks.IsEmpty(); };

		explicit THeuristicsHandler(FPCGExPointsProcessorContext* InContext, double InReferenceWeight = 100);
		explicit THeuristicsHandler(UPCGExHeuristicOperation* InSingleOperation);
		~THeuristicsHandler();

		bool PrepareForCluster(FPCGExAsyncManager* AsyncManager, PCGExCluster::FCluster* InCluster);
		void CompleteClusterPreparation();

		double GetGlobalScore(
			const PCGExCluster::FNode& From,
			const PCGExCluster::FNode& Seed,
			const PCGExCluster::FNode& Goal,
			const FLocalFeedbackHandler* LocalFeedback = nullptr) const;

		double GetEdgeScore(
			const PCGExCluster::FNode& From,
			const PCGExCluster::FNode& To,
			const PCGExGraph::FIndexedEdge& Edge,
			const PCGExCluster::FNode& Seed,
			const PCGExCluster::FNode& Goal,
			const FLocalFeedbackHandler* LocalFeedback = nullptr) const;

		void FeedbackPointScore(const PCGExCluster::FNode& Node);
		void FeedbackScore(const PCGExCluster::FNode& Node, const PCGExGraph::FIndexedEdge& Edge);

		FLocalFeedbackHandler* MakeLocalFeedbackHandler(PCGExCluster::FCluster* InCluster);
	};
}

namespace PCGExHeuristicsTasks
{
	class PCGEXTENDEDTOOLKIT_API FCompileModifiers : public FPCGExNonAbandonableTask
	{
	public:
		FCompileModifiers(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		                  PCGExData::FPointIO* InEdgeIO,
		                  FPCGExHeuristicModifiersSettings* InModifiers) :
			FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			EdgeIO(InEdgeIO),
			Modifiers(InModifiers)
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
}
