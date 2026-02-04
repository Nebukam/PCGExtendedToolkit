// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Helpers/PCGExClusterHelpers.h"
#include "Clusters/PCGExLink.h"

namespace PCGExTest
{
	/**
	 * Test version of FNodeChain that works with FTestCluster
	 */
	struct PCGEXTENDEDTOOLKITTEST_API FTestChain
	{
		PCGExGraphs::FLink Seed;
		int32 SingleEdge = -1;

		bool bIsClosedLoop = false;
		bool bIsLeaf = false;

		uint64 UniqueHash = 0;
		TArray<PCGExGraphs::FLink> Links;

		explicit FTestChain(const PCGExGraphs::FLink InSeed)
			: Seed(InSeed)
		{
		}

		void FixUniqueHash();
		void BuildChain(const TSharedRef<FTestCluster>& Cluster, const TSharedPtr<TArray<int8>>& Breakpoints);

		/** Get all node indices in the chain */
		void GetNodeIndices(TArray<int32>& OutIndices, bool bReverse = false) const;
	};

	/**
	 * Test chain building helpers
	 */
	namespace TestChainHelpers
	{
		/**
		 * Build all chains from a test cluster
		 * @param Cluster The test cluster
		 * @param OutChains Output array of chains
		 * @param Breakpoints Optional breakpoints array (indexed by PointIndex)
		 * @return True if any chains were built
		 */
		PCGEXTENDEDTOOLKITTEST_API bool BuildChains(
			const TSharedRef<FTestCluster>& Cluster,
			TArray<TSharedPtr<FTestChain>>& OutChains,
			const TSharedPtr<TArray<int8>>& Breakpoints = nullptr);

		/**
		 * Apply breakpoints to existing chains, splitting them as needed
		 * @param SourceChains Input chains to split
		 * @param Cluster The test cluster
		 * @param Breakpoints Breakpoints array (indexed by PointIndex)
		 * @param OutChains Output array of split chains
		 */
		PCGEXTENDEDTOOLKITTEST_API void ApplyBreakpoints(
			const TArray<TSharedPtr<FTestChain>>& SourceChains,
			const TSharedRef<FTestCluster>& Cluster,
			const TSharedPtr<TArray<int8>>& Breakpoints,
			TArray<TSharedPtr<FTestChain>>& OutChains);

		/**
		 * Filter chains to only include leaf chains
		 * @param SourceChains Input chains
		 * @param OutChains Output array (can be same as source for in-place)
		 */
		PCGEXTENDEDTOOLKITTEST_API void FilterLeavesOnly(
			const TArray<TSharedPtr<FTestChain>>& SourceChains,
			TArray<TSharedPtr<FTestChain>>& OutChains);

		/**
		 * Count chains with specific characteristics
		 */
		PCGEXTENDEDTOOLKITTEST_API int32 CountLeafChains(const TArray<TSharedPtr<FTestChain>>& Chains);
		PCGEXTENDEDTOOLKITTEST_API int32 CountClosedLoops(const TArray<TSharedPtr<FTestChain>>& Chains);
		PCGEXTENDEDTOOLKITTEST_API int32 CountSingleEdgeChains(const TArray<TSharedPtr<FTestChain>>& Chains);
	}
}
