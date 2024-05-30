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

namespace PCGExHeuristics
{
	struct PCGEXTENDEDTOOLKIT_API FModifiersSettings
	{
		double TotalWeight = 0;
		double ReferenceWeight = 1;

		TArray<FPCGExHeuristicModifierDescriptor> Modifiers;

		PCGExData::FPointIO* LastPoints = nullptr;
		TArray<double> PointScoreModifiers;
		TArray<double> EdgeScoreModifiers;

		FModifiersSettings()
		{
			PointScoreModifiers.Empty();
			EdgeScoreModifiers.Empty();
		}

		~FModifiersSettings()
		{
			Cleanup();
		}

		void LoadCurves()
		{
			for (FPCGExHeuristicModifierDescriptor& Modifier : Modifiers)
			{
				PCGEX_LOAD_SOFTOBJECT(UCurveFloat, Modifier.ScoreCurve, Modifier.ScoreCurveObj, PCGEx::WeightDistributionLinear)
			}
		}

		void Cleanup()
		{
			LastPoints = nullptr;
			PointScoreModifiers.Empty();
			EdgeScoreModifiers.Empty();
		}

		void PrepareForData(PCGExData::FPointIO& InPoints, PCGExData::FPointIO& InEdges)
		{
			bool bUpdatePoints = false;
			const int32 NumPoints = InPoints.GetNum();
			const int32 NumEdges = InEdges.GetNum();

			TotalWeight = 0;

			if (LastPoints != &InPoints)
			{
				LastPoints = &InPoints;
				bUpdatePoints = true;

				InPoints.CreateInKeys();
				PointScoreModifiers.SetNumZeroed(NumPoints);
			}

			InEdges.CreateInKeys();
			EdgeScoreModifiers.SetNumZeroed(NumEdges);

			if (Modifiers.IsEmpty()) { return; }

			TArray<double>* TargetArray;
			int32 NumIterations;

			for (const FPCGExHeuristicModifierDescriptor& Modifier : Modifiers)
			{
				TotalWeight += Modifier.WeightFactor;

				PCGEx::FLocalSingleFieldGetter* ModifierGetter = new PCGEx::FLocalSingleFieldGetter();
				ModifierGetter->Capture(Modifier.Attribute);

				const TObjectPtr<UCurveFloat> ScoreFC = Modifier.ScoreCurveObj;

				bool bModifierGrabbed;
				if (Modifier.Source == EPCGExGraphValueSource::Point)
				{
					if (!bUpdatePoints) { continue; }
					bModifierGrabbed = ModifierGetter->Grab(InPoints, true);
					TargetArray = &PointScoreModifiers;
					NumIterations = NumPoints;
				}
				else
				{
					bModifierGrabbed = ModifierGetter->Grab(InEdges, true);
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

				const double OutMin = Modifier.bInvert ? 1 : 0;
				const double OutMax = Modifier.bInvert ? 0 : 1;


				const double Factor = ReferenceWeight * Modifier.WeightFactor;
				for (int i = 0; i < NumIterations; i++)
				{
					const double NormalizedValue = PCGExMath::Remap(ModifierGetter->Values[i], MinValue, MaxValue, OutMin, OutMax);
					(*TargetArray)[i] += FMath::Max(0, ScoreFC->GetFloatValue(NormalizedValue)) * Factor;
				}


				PCGEX_DELETE(ModifierGetter)
			}
		}

		double GetScore(const int32 PointIndex, const int32 EdgeIndex) const
		{
			return PointScoreModifiers[PointIndex] + EdgeScoreModifiers[EdgeIndex];
		}
	};

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
		FModifiersSettings* HeuristicsModifiers = nullptr;
		TArray<UPCGExHeuristicOperation*> Operations;
		TArray<UPCGExHeuristicFeedback*> Feedbacks;
		TArray<UPCGHeuristicsFactoryBase*> LocalFeedbackFactories;

		PCGExCluster::FCluster* CurrentCluster = nullptr;

		double ReferenceWeight = 1;
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
		                  PCGExHeuristics::FModifiersSettings* InModifiers) :
			FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			EdgeIO(InEdgeIO),
			Modifiers(InModifiers)
		{
		}

		PCGExData::FPointIO* EdgeIO = nullptr;
		PCGExHeuristics::FModifiersSettings* Modifiers = nullptr;

		virtual bool ExecuteTask() override
		{
			Modifiers->PrepareForData(*PointIO, *EdgeIO);
			return true;
		}
	};
}
