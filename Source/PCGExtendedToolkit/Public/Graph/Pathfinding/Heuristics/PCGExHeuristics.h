// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExHeuristicFeedback.h"
#include "PCGExHeuristicsFactoryProvider.h"

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
	struct PCGEXTENDEDTOOLKIT_API FLocalFeedbackHandler
	{
		PCGExData::FFacade* VtxDataFacade = nullptr;
		PCGExData::FFacade* EdgeDataFacade = nullptr;

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
				PCGEX_DELETE_OPERATION(Feedback)
			}

			Feedbacks.Empty();
		}

		FORCEINLINE double GetGlobalScore(
			const PCGExCluster::FNode& From,
			const PCGExCluster::FNode& Seed,
			const PCGExCluster::FNode& Goal) const
		{
			double GScore = 0;
			for (const UPCGExHeuristicFeedback* Feedback : Feedbacks) { GScore += Feedback->GetGlobalScore(From, Seed, Goal); }
			return GScore;
		}

		FORCEINLINE double GetEdgeScore(
			const PCGExCluster::FNode& From,
			const PCGExCluster::FNode& To,
			const PCGExGraph::FIndexedEdge& Edge,
			const PCGExCluster::FNode& Seed,
			const PCGExCluster::FNode& Goal) const
		{
			double EScore = 0;
			for (const UPCGExHeuristicFeedback* Feedback : Feedbacks) { EScore += Feedback->GetEdgeScore(From, To, Edge, Seed, Goal); }
			return EScore;
		}

		FORCEINLINE void FeedbackPointScore(const PCGExCluster::FNode& Node)
		{
			for (UPCGExHeuristicFeedback* Feedback : Feedbacks) { Feedback->FeedbackPointScore(Node); }
		}

		FORCEINLINE void FeedbackScore(const PCGExCluster::FNode& Node, const PCGExGraph::FIndexedEdge& Edge)
		{
			for (UPCGExHeuristicFeedback* Feedback : Feedbacks) { Feedback->FeedbackScore(Node, Edge); }
		}
	};

	class PCGEXTENDEDTOOLKIT_API THeuristicsHandler
	{
	public:
		PCGExData::FFacade* VtxDataFacade = nullptr;
		PCGExData::FFacade* EdgeDataFacade = nullptr;

		TArray<UPCGExHeuristicOperation*> Operations;
		TArray<UPCGExHeuristicFeedback*> Feedbacks;
		TArray<const UPCGExHeuristicsFactoryBase*> LocalFeedbackFactories;

		PCGExCluster::FCluster* CurrentCluster = nullptr;

		double ReferenceWeight = 1;
		double TotalStaticWeight = 0;
		bool bUseDynamicWeight = false;

		bool HasGlobalFeedback() const { return !Feedbacks.IsEmpty(); };

		explicit THeuristicsHandler(FPCGContext* InContext, PCGExData::FFacade* InVtxDataFacade, PCGExData::FFacade* InEdgeDataFacade);
		explicit THeuristicsHandler(FPCGContext* InContext, PCGExData::FFacade* InVtxDataCache, PCGExData::FFacade* InEdgeDataCache, const TArray<UPCGExHeuristicsFactoryBase*>& InFactories);
		~THeuristicsHandler();

		void BuildFrom(FPCGContext* InContext, const TArray<UPCGExHeuristicsFactoryBase*>& InFactories);
		void PrepareForCluster(PCGExCluster::FCluster* InCluster);
		void CompleteClusterPreparation();

		FORCEINLINE double GetGlobalScore(
			const PCGExCluster::FNode& From,
			const PCGExCluster::FNode& Seed,
			const PCGExCluster::FNode& Goal,
			const FLocalFeedbackHandler* LocalFeedback = nullptr) const
		{
			//TODO : Account for custom weight here
			double GScore = 0;
			for (const UPCGExHeuristicOperation* Op : Operations) { GScore += Op->GetGlobalScore(From, Seed, Goal); }
			if (LocalFeedback) { return (GScore + LocalFeedback->GetGlobalScore(From, Seed, Goal)) / (TotalStaticWeight + LocalFeedback->TotalWeight); }
			return GScore / TotalStaticWeight;
		}

		FORCEINLINE double GetEdgeScore(
			const PCGExCluster::FNode& From,
			const PCGExCluster::FNode& To,
			const PCGExGraph::FIndexedEdge& Edge,
			const PCGExCluster::FNode& Seed,
			const PCGExCluster::FNode& Goal,
			const FLocalFeedbackHandler* LocalFeedback = nullptr) const
		{
			//TODO : Account for custom weight here
			double EScore = 0;
			if (!bUseDynamicWeight)
			{
				for (const UPCGExHeuristicOperation* Op : Operations) { EScore += Op->GetEdgeScore(From, To, Edge, Seed, Goal); }
				if (LocalFeedback) { return (EScore + LocalFeedback->GetEdgeScore(From, To, Edge, Seed, Goal)) / (TotalStaticWeight + LocalFeedback->TotalWeight); }
				return EScore / TotalStaticWeight;
			}

			double DynamicWeight = 0;
			for (const UPCGExHeuristicOperation* Op : Operations)
			{
				EScore += Op->GetEdgeScore(From, To, Edge, Seed, Goal);
				DynamicWeight += (Op->WeightFactor * Op->GetCustomWeightMultiplier(To.NodeIndex, Edge.PointIndex));
			}
			if (LocalFeedback) { return (EScore + LocalFeedback->GetEdgeScore(From, To, Edge, Seed, Goal)) / (DynamicWeight + LocalFeedback->TotalWeight); }
			return EScore / DynamicWeight;
		}

		FORCEINLINE void FeedbackPointScore(const PCGExCluster::FNode& Node)
		{
			for (UPCGExHeuristicFeedback* Op : Feedbacks) { Op->FeedbackPointScore(Node); }
		}

		FORCEINLINE void FeedbackScore(const PCGExCluster::FNode& Node, const PCGExGraph::FIndexedEdge& Edge)
		{
			for (UPCGExHeuristicFeedback* Op : Feedbacks) { Op->FeedbackScore(Node, Edge); }
		}

		FLocalFeedbackHandler* MakeLocalFeedbackHandler(const PCGExCluster::FCluster* InCluster);
	};
}
