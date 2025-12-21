// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExHeuristicsFactoryProvider.h"
#include "Clusters/PCGExNode.h"

class FPCGExHeuristicFeedback;

namespace PCGEx
{
	class FHashLookup;
}

namespace PCGExGraph
{
	struct FEdge;
}

namespace PCGExHeuristics
{
	class PCGEXHEURISTICS_API FLocalFeedbackHandler : public TSharedFromThis<FLocalFeedbackHandler>
	{
	public:
		FPCGExContext* ExecutionContext = nullptr;

		TSharedPtr<PCGExData::FFacade> VtxDataFacade;
		TSharedPtr<PCGExData::FFacade> EdgeDataFacade;

		TArray<TSharedPtr<FPCGExHeuristicFeedback>> Feedbacks;
		double TotalStaticWeight = 0;

		explicit FLocalFeedbackHandler(FPCGExContext* InContext)
			: ExecutionContext(InContext)
		{
		}

		~FLocalFeedbackHandler() = default;

		double GetGlobalScore(const PCGExCluster::FNode& From, const PCGExCluster::FNode& Seed, const PCGExCluster::FNode& Goal) const;

		double GetEdgeScore(const PCGExCluster::FNode& From, const PCGExCluster::FNode& To, const PCGExGraph::FEdge& Edge, const PCGExCluster::FNode& Seed, const PCGExCluster::FNode& Goal, const TSharedPtr<PCGEx::FHashLookup>& TravelStack = nullptr) const;

		void FeedbackPointScore(const PCGExCluster::FNode& Node);

		void FeedbackScore(const PCGExCluster::FNode& Node, const PCGExGraph::FEdge& Edge);
	};

	class PCGEXHEURISTICS_API FHandler : public TSharedFromThis<FHandler>
	{
		FPCGExContext* ExecutionContext = nullptr;
		bool bIsValidHandler = false;

	public:
		mutable FRWLock HandlerLock;
		TSharedPtr<PCGExData::FFacade> VtxDataFacade;
		TSharedPtr<PCGExData::FFacade> EdgeDataFacade;

		TArray<TSharedPtr<FPCGExHeuristicOperation>> Operations;
		TArray<TSharedPtr<FPCGExHeuristicFeedback>> Feedbacks;
		TArray<TObjectPtr<const UPCGExHeuristicsFactoryData>> LocalFeedbackFactories;

		TSharedPtr<PCGExCluster::FCluster> Cluster;

		double ReferenceWeight = 1;
		double TotalStaticWeight = 0;
		bool bUseDynamicWeight = false;

		bool IsValidHandler() const { return bIsValidHandler; }
		bool HasGlobalFeedback() const { return !Feedbacks.IsEmpty(); };
		bool HasLocalFeedback() const { return !LocalFeedbackFactories.IsEmpty(); };
		bool HasAnyFeedback() const { return HasGlobalFeedback() || HasLocalFeedback(); };

		FHandler(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InVtxDataCache, const TSharedPtr<PCGExData::FFacade>& InEdgeDataCache, const TArray<TObjectPtr<const UPCGExHeuristicsFactoryData>>& InFactories);
		~FHandler();

		bool BuildFrom(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExHeuristicsFactoryData>>& InFactories);
		void PrepareForCluster(const TSharedPtr<PCGExCluster::FCluster>& InCluster);
		void CompleteClusterPreparation();


		double GetGlobalScore(const PCGExCluster::FNode& From, const PCGExCluster::FNode& Seed, const PCGExCluster::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback = nullptr) const;


		double GetEdgeScore(const PCGExCluster::FNode& From, const PCGExCluster::FNode& To, const PCGExGraph::FEdge& Edge, const PCGExCluster::FNode& Seed, const PCGExCluster::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback = nullptr, const TSharedPtr<PCGEx::FHashLookup>& TravelStack = nullptr) const;

		void FeedbackPointScore(const PCGExCluster::FNode& Node);
		void FeedbackScore(const PCGExCluster::FNode& Node, const PCGExGraph::FEdge& Edge);

		FVector GetSeedUVW() const;
		FVector GetGoalUVW() const;

		const PCGExCluster::FNode* GetRoamingSeed();
		const PCGExCluster::FNode* GetRoamingGoal();

		TSharedPtr<FLocalFeedbackHandler> MakeLocalFeedbackHandler(const TSharedPtr<const PCGExCluster::FCluster>& InCluster);

	protected:
		PCGExCluster::FNode* RoamingSeedNode = nullptr;
		PCGExCluster::FNode* RoamingGoalNode = nullptr;
	};
}
