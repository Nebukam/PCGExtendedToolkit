// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Clusters/PCGExClusterCache.h"

namespace PCGExClusters
{
	class FNodeChain;
	class FCluster;

	/**
	 * Cached chain data.
	 * Stores pre-built chains representing the base cluster topology (no breakpoints).
	 */
	class PCGEXGRAPHS_API FCachedChainData : public ICachedClusterData
	{
	public:
		TArray<TSharedPtr<FNodeChain>> Chains;
	};

	/**
	 * Factory for pre-building chain cache.
	 */
	class PCGEXGRAPHS_API FChainCacheFactory : public IClusterCacheFactory
	{
	public:
		static inline const FName CacheKey = FName("NodeChains");

		virtual FName GetCacheKey() const override { return CacheKey; }
		virtual FText GetDisplayName() const override;
		virtual FText GetTooltip() const override;
		virtual EClusterCacheType GetCacheType() const override { return EClusterCacheType::PreBuild; }

		virtual TSharedPtr<ICachedClusterData> Build(const FClusterCacheBuildContext& Context) const override;
	};

	/**
	 * Chain building and caching utilities.
	 * Similar pattern to FaceEnumerator - provides GetOrBuildChains for easy consumption.
	 */
	namespace ChainHelpers
	{
		/**
		 * Get or build chains for a cluster.
		 * - Checks cluster cache first
		 * - If not cached, builds synchronously (with parallel loop) and caches opportunistically
		 * - Applies breakpoints if provided
		 * - Filters to leaves-only if requested
		 *
		 * @param Cluster - The cluster to get/build chains for
		 * @param OutChains - Output array of processed chains
		 * @param Breakpoints - Optional breakpoint flags indexed by PointIndex (splits chains at these nodes)
		 * @param bLeavesOnly - If true, only return chains that start/end at leaf nodes
		 * @return true if chains were successfully retrieved/built
		 */
		PCGEXGRAPHS_API bool GetOrBuildChains(
			const TSharedRef<FCluster>& Cluster,
			TArray<TSharedPtr<FNodeChain>>& OutChains,
			const TSharedPtr<TArray<int8>>& Breakpoints = nullptr,
			bool bLeavesOnly = false);

		/**
		 * Build chains synchronously and cache them.
		 * Called internally by GetOrBuildChains on cache miss.
		 * Uses parallel loop for chain building.
		 *
		 * @param Cluster - The cluster to build chains for
		 * @return Cached chain data, or nullptr if no chains could be built
		 */
		PCGEXGRAPHS_API TSharedPtr<FCachedChainData> BuildAndCacheChains(const TSharedRef<FCluster>& Cluster);

		/**
		 * Apply breakpoints to cached chains, splitting them at breakpoint nodes.
		 * Produces new chains with updated bIsLeaf/bIsClosedLoop flags.
		 *
		 * @param SourceChains - The cached base chains (no breakpoints)
		 * @param Cluster - The cluster for topology lookups
		 * @param Breakpoints - Array of breakpoint flags indexed by PointIndex
		 * @param OutChains - Output array of split chains
		 */
		PCGEXGRAPHS_API void ApplyBreakpoints(
			const TArray<TSharedPtr<FNodeChain>>& SourceChains,
			const TSharedRef<FCluster>& Cluster,
			const TSharedPtr<TArray<int8>>& Breakpoints,
			TArray<TSharedPtr<FNodeChain>>& OutChains);

		/**
		 * Filter chains to only those originating from leaf nodes.
		 *
		 * @param SourceChains - Input chains
		 * @param OutChains - Output filtered chains (leaf-originating only)
		 */
		PCGEXGRAPHS_API void FilterLeavesOnly(
			const TArray<TSharedPtr<FNodeChain>>& SourceChains,
			TArray<TSharedPtr<FNodeChain>>& OutChains);
	}
}
