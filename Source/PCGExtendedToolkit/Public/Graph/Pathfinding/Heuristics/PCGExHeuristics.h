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

		PCGExData::FPool* VtxDataCache = nullptr;
		PCGExData::FPool* EdgeDataCache = nullptr;
		
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
		PCGExData::FPool* VtxDataCache = nullptr;
		PCGExData::FPool* EdgeDataCache = nullptr;
		
		TArray<UPCGExHeuristicOperation*> Operations;
		TArray<UPCGExHeuristicFeedback*> Feedbacks;
		TArray<const UPCGExHeuristicsFactoryBase*> LocalFeedbackFactories;

		PCGExCluster::FCluster* CurrentCluster = nullptr;

		double ReferenceWeight = 1;
		double TotalStaticWeight = 0;
		double bUseDynamicWeight = false;

		bool HasGlobalFeedback() const { return !Feedbacks.IsEmpty(); };

		explicit THeuristicsHandler(FPCGContext* InContext, PCGExData::FPool* InVtxDataCache, PCGExData::FPool* InEdgeDataCache);
		explicit THeuristicsHandler(FPCGContext* InContext, PCGExData::FPool* InVtxDataCache, PCGExData::FPool* InEdgeDataCache, const TArray<UPCGExHeuristicsFactoryBase*>& InFactories);
		~THeuristicsHandler();

		void BuildFrom(FPCGContext* InContext, const TArray<UPCGExHeuristicsFactoryBase*>& InFactories);
		void PrepareForCluster(PCGExCluster::FCluster* InCluster);
		void CompleteClusterPreparation();

		FORCEINLINE double GetGlobalScore(
			const PCGExCluster::FNode& From,
			const PCGExCluster::FNode& Seed,
			const PCGExCluster::FNode& Goal,
			const FLocalFeedbackHandler* LocalFeedback = nullptr) const;

		FORCEINLINE double GetEdgeScore(
			const PCGExCluster::FNode& From,
			const PCGExCluster::FNode& To,
			const PCGExGraph::FIndexedEdge& Edge,
			const PCGExCluster::FNode& Seed,
			const PCGExCluster::FNode& Goal,
			const FLocalFeedbackHandler* LocalFeedback = nullptr) const;

		FORCEINLINE void FeedbackPointScore(const PCGExCluster::FNode& Node);
		FORCEINLINE void FeedbackScore(const PCGExCluster::FNode& Node, const PCGExGraph::FIndexedEdge& Edge);

		FLocalFeedbackHandler* MakeLocalFeedbackHandler(const PCGExCluster::FCluster* InCluster);
	};
}
