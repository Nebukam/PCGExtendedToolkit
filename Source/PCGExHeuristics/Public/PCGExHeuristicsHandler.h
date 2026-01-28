// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExHeuristicsFactoryProvider.h"
#include "PCGExHeuristicsCommon.h"
#include "Clusters/PCGExNode.h"

class FPCGExHeuristicFeedback;
class FPCGExHeuristicOperation;
enum class EPCGExHeuristicCategory : uint8;

namespace PCGEx
{
	class FHashLookup;
}

namespace PCGExGraphs
{
	struct FEdge;
}

namespace PCGExHeuristics
{
	/** Categorized operation arrays for fast-path optimizations */
	struct PCGEXHEURISTICS_API FCategorizedOperations
	{
		TArray<TSharedPtr<FPCGExHeuristicOperation>> FullyStatic;
		TArray<TSharedPtr<FPCGExHeuristicOperation>> GoalDependent;
		TArray<TSharedPtr<FPCGExHeuristicOperation>> TravelDependent;
		// Note: Feedback operations are stored separately in Feedbacks array

		double FullyStaticWeight = 0;
		double GoalDependentWeight = 0;
		double TravelDependentWeight = 0;

		bool bHasTravelDependent = false;

		void Reset()
		{
			FullyStatic.Empty();
			GoalDependent.Empty();
			TravelDependent.Empty();
			FullyStaticWeight = 0;
			GoalDependentWeight = 0;
			TravelDependentWeight = 0;
			bHasTravelDependent = false;
		}
	};

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

		double GetGlobalScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal) const;

		double GetEdgeScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& To, const PCGExGraphs::FEdge& Edge, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const TSharedPtr<PCGEx::FHashLookup>& TravelStack = nullptr) const;

		void FeedbackPointScore(const PCGExClusters::FNode& Node);

		void FeedbackScore(const PCGExClusters::FNode& Node, const PCGExGraphs::FEdge& Edge);

		/** Reset all feedback counts for reuse */
		void ResetFeedback();
	};

	/**
	 * Base handler class for heuristics. Subclasses implement different score aggregation modes.
	 */
	class PCGEXHEURISTICS_API FHandler : public TSharedFromThis<FHandler>
	{
	protected:
		FPCGExContext* ExecutionContext = nullptr;
		bool bIsValidHandler = false;

	public:
		mutable FRWLock HandlerLock;
		TSharedPtr<PCGExData::FFacade> VtxDataFacade;
		TSharedPtr<PCGExData::FFacade> EdgeDataFacade;

		TArray<TSharedPtr<FPCGExHeuristicOperation>> Operations;
		TArray<TSharedPtr<FPCGExHeuristicFeedback>> Feedbacks;
		TArray<TObjectPtr<const UPCGExHeuristicsFactoryData>> LocalFeedbackFactories;

		TSharedPtr<PCGExClusters::FCluster> Cluster;

		double ReferenceWeight = 1;
		double TotalStaticWeight = 0;
		bool bUseDynamicWeight = false;

		/** Categorized operations for fast-path optimizations */
		FCategorizedOperations CategorizedOps;

		bool IsValidHandler() const { return bIsValidHandler; }
		bool HasTravelDependentOperations() const { return CategorizedOps.bHasTravelDependent; }
		bool HasGlobalFeedback() const { return !Feedbacks.IsEmpty(); };
		bool HasLocalFeedback() const { return !LocalFeedbackFactories.IsEmpty(); };
		bool HasAnyFeedback() const { return HasGlobalFeedback() || HasLocalFeedback(); };

		FHandler(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InVtxDataCache, const TSharedPtr<PCGExData::FFacade>& InEdgeDataCache, const TArray<TObjectPtr<const UPCGExHeuristicsFactoryData>>& InFactories);
		virtual ~FHandler();

		bool BuildFrom(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExHeuristicsFactoryData>>& InFactories);
		void PrepareForCluster(const TSharedPtr<PCGExClusters::FCluster>& InCluster);
		void CompleteClusterPreparation();

		/** Override in subclasses to implement different score aggregation modes */
		virtual double GetGlobalScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback = nullptr) const = 0;

		/** Override in subclasses to implement different score aggregation modes */
		virtual double GetEdgeScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& To, const PCGExGraphs::FEdge& Edge, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback = nullptr, const TSharedPtr<PCGEx::FHashLookup>& TravelStack = nullptr) const = 0;

		void FeedbackPointScore(const PCGExClusters::FNode& Node);
		void FeedbackScore(const PCGExClusters::FNode& Node, const PCGExGraphs::FEdge& Edge);

		FVector GetSeedUVW() const;
		FVector GetGoalUVW() const;

		const PCGExClusters::FNode* GetRoamingSeed();
		const PCGExClusters::FNode* GetRoamingGoal();

		TSharedPtr<FLocalFeedbackHandler> MakeLocalFeedbackHandler(const TSharedPtr<const PCGExClusters::FCluster>& InCluster);

		/** Acquire a local feedback handler from pool (creates new if pool is empty) */
		TSharedPtr<FLocalFeedbackHandler> AcquireLocalFeedbackHandler(const TSharedPtr<const PCGExClusters::FCluster>& InCluster);

		/** Release a local feedback handler back to pool for reuse */
		void ReleaseLocalFeedbackHandler(const TSharedPtr<FLocalFeedbackHandler>& Handler);

		/** Factory function to create a handler with the specified scoring mode */
		static TSharedPtr<FHandler> CreateHandler(
			EPCGExHeuristicScoreMode ScoreMode,
			FPCGExContext* InContext,
			const TSharedPtr<PCGExData::FFacade>& InVtxDataCache,
			const TSharedPtr<PCGExData::FFacade>& InEdgeDataCache,
			const TArray<TObjectPtr<const UPCGExHeuristicsFactoryData>>& InFactories);

	protected:
		PCGExClusters::FNode* RoamingSeedNode = nullptr;
		PCGExClusters::FNode* RoamingGoalNode = nullptr;

		/** Pool of reusable local feedback handlers */
		TArray<TSharedPtr<FLocalFeedbackHandler>> LocalFeedbackHandlerPool;
		FCriticalSection PoolLock;
	};

	//
	// Concrete handler implementations
	//

	/** Weighted average: sum(score × weight) / sum(weight) */
	class PCGEXHEURISTICS_API FHandlerWeightedAverage final : public FHandler
	{
	public:
		using FHandler::FHandler;

		virtual double GetGlobalScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback = nullptr) const override;
		virtual double GetEdgeScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& To, const PCGExGraphs::FEdge& Edge, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback = nullptr, const TSharedPtr<PCGEx::FHashLookup>& TravelStack = nullptr) const override;
	};

	/** Geometric mean: product(score^weight)^(1/sum(weight)) */
	class PCGEXHEURISTICS_API FHandlerGeometricMean final : public FHandler
	{
	public:
		using FHandler::FHandler;

		virtual double GetGlobalScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback = nullptr) const override;
		virtual double GetEdgeScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& To, const PCGExGraphs::FEdge& Edge, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback = nullptr, const TSharedPtr<PCGEx::FHashLookup>& TravelStack = nullptr) const override;
	};

	/** Weighted sum: sum(score × weight) - no normalization */
	class PCGEXHEURISTICS_API FHandlerWeightedSum final : public FHandler
	{
	public:
		using FHandler::FHandler;

		virtual double GetGlobalScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback = nullptr) const override;
		virtual double GetEdgeScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& To, const PCGExGraphs::FEdge& Edge, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback = nullptr, const TSharedPtr<PCGEx::FHashLookup>& TravelStack = nullptr) const override;
	};

	/** Harmonic mean: sum(weight) / sum(weight/score) - heavily emphasizes low scores */
	class PCGEXHEURISTICS_API FHandlerHarmonicMean final : public FHandler
	{
	public:
		using FHandler::FHandler;

		virtual double GetGlobalScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback = nullptr) const override;
		virtual double GetEdgeScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& To, const PCGExGraphs::FEdge& Edge, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback = nullptr, const TSharedPtr<PCGEx::FHashLookup>& TravelStack = nullptr) const override;
	};

	/** Minimum: returns the lowest weighted score - most permissive */
	class PCGEXHEURISTICS_API FHandlerMin final : public FHandler
	{
	public:
		using FHandler::FHandler;

		virtual double GetGlobalScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback = nullptr) const override;
		virtual double GetEdgeScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& To, const PCGExGraphs::FEdge& Edge, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback = nullptr, const TSharedPtr<PCGEx::FHashLookup>& TravelStack = nullptr) const override;
	};

	/** Maximum: returns the highest weighted score - most restrictive */
	class PCGEXHEURISTICS_API FHandlerMax final : public FHandler
	{
	public:
		using FHandler::FHandler;

		virtual double GetGlobalScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback = nullptr) const override;
		virtual double GetEdgeScore(const PCGExClusters::FNode& From, const PCGExClusters::FNode& To, const PCGExGraphs::FEdge& Edge, const PCGExClusters::FNode& Seed, const PCGExClusters::FNode& Goal, const FLocalFeedbackHandler* LocalFeedback = nullptr, const TSharedPtr<PCGEx::FHashLookup>& TravelStack = nullptr) const override;
	};
}
