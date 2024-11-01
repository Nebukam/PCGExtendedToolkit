// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExHeuristicFeedback.h"
#include "PCGExHeuristicsFactoryProvider.h"


#include "Graph/Pathfinding/PCGExPathfinding.h"

#include "PCGExHeuristics.generated.h"

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Heuristic Score Mode")--E*/)
enum class EPCGExHeuristicScoreMode : uint8
{
	LowerIsBetter  = 0 UMETA(DisplayName = "Lower is Better", Tooltip="Lower values are considered more desirable."),
	HigherIsBetter = 1 UMETA(DisplayName = "Higher is Better", Tooltip="Higher values are considered more desirable."),
};

namespace PCGExHeuristics
{
	struct /*PCGEXTENDEDTOOLKIT_API*/ FLocalFeedbackHandler
	{
		FPCGExContext* ExecutionContext = nullptr;

		TSharedPtr<PCGExData::FFacade> VtxDataFacade;
		TSharedPtr<PCGExData::FFacade> EdgeDataFacade;

		TArray<UPCGExHeuristicFeedback*> Feedbacks;
		double TotalWeight = 0;

		explicit FLocalFeedbackHandler(FPCGExContext* InContext):
			ExecutionContext(InContext)
		{
		}

		~FLocalFeedbackHandler()
		{
			for (UPCGExHeuristicFeedback* Feedback : Feedbacks) { ExecutionContext->ManagedObjects->Destroy(Feedback); }
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
			const PCGExCluster::FNode& Goal,
			const TArray<uint64>* TravelStack = nullptr) const
		{
			double EScore = 0;
			for (const UPCGExHeuristicFeedback* Feedback : Feedbacks) { EScore += Feedback->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack); }
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

	class /*PCGEXTENDEDTOOLKIT_API*/ THeuristicsHandler
	{
		FPCGExContext* ExecutionContext = nullptr;

	public:
		TSharedPtr<PCGExData::FFacade> VtxDataFacade;
		TSharedPtr<PCGExData::FFacade> EdgeDataFacade;

		TArray<UPCGExHeuristicOperation*> Operations;
		TArray<UPCGExHeuristicFeedback*> Feedbacks;
		TArray<TObjectPtr<const UPCGExHeuristicsFactoryBase>> LocalFeedbackFactories;

		TSharedPtr<PCGExCluster::FCluster> Cluster;

		double ReferenceWeight = 1;
		double TotalStaticWeight = 0;
		bool bUseDynamicWeight = false;

		bool HasGlobalFeedback() const { return !Feedbacks.IsEmpty(); };

		explicit THeuristicsHandler(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InVtxDataFacade, const TSharedPtr<PCGExData::FFacade>& InEdgeDataFacade);
		explicit THeuristicsHandler(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InVtxDataCache, const TSharedPtr<PCGExData::FFacade>& InEdgeDataCache, const TArray<TObjectPtr<const UPCGExHeuristicsFactoryBase>>& InFactories);
		~THeuristicsHandler();

		void BuildFrom(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExHeuristicsFactoryBase>>& InFactories);
		void PrepareForCluster(const TSharedPtr<PCGExCluster::FCluster>& InCluster);
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
			const FLocalFeedbackHandler* LocalFeedback = nullptr,
			const TArray<uint64>* TravelStack = nullptr) const
		{
			double EScore = 0;
			if (!bUseDynamicWeight)
			{
				for (const UPCGExHeuristicOperation* Op : Operations) { EScore += Op->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack); }
				if (LocalFeedback) { return (EScore + LocalFeedback->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack)) / (TotalStaticWeight + LocalFeedback->TotalWeight); }
				return EScore / TotalStaticWeight;
			}

			double DynamicWeight = 0;
			for (const UPCGExHeuristicOperation* Op : Operations)
			{
				EScore += Op->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack);
				DynamicWeight += (Op->WeightFactor * Op->GetCustomWeightMultiplier(To.NodeIndex, Edge.PointIndex));
			}
			if (LocalFeedback) { return (EScore + LocalFeedback->GetEdgeScore(From, To, Edge, Seed, Goal, TravelStack)) / (DynamicWeight + LocalFeedback->TotalWeight); }
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

		TSharedPtr<FLocalFeedbackHandler> MakeLocalFeedbackHandler(const TSharedPtr<const PCGExCluster::FCluster>& InCluster);
	};
}
