// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExValencyCommon.h"

namespace PCGExClusters
{
	class FCluster;
}

namespace PCGExData
{
	template <typename T>
	class TBuffer;
}

namespace PCGExValency
{
	/**
	 * Cached orbital-to-neighbor mappings for a cluster.
	 * Built from cluster topology + edge orbital attributes.
	 * Uses flat array storage for cache-efficient access.
	 */
	class PCGEXELEMENTSVALENCY_API FOrbitalCache : public TSharedFromThis<FOrbitalCache>
	{
	public:
		FOrbitalCache() = default;
		~FOrbitalCache() = default;

		/**
		 * Build cache from cluster data and orbital attributes (via buffer readers).
		 * @param Cluster The cluster providing topology (nodes, edges, links)
		 * @param OrbitalMaskReader Vertex attribute reader for orbital masks
		 * @param EdgeIndicesReader Edge attribute reader for packed orbital indices
		 * @param InMaxOrbitals Maximum orbital count from OrbitalSet
		 * @return True if cache was built successfully
		 */
		bool BuildFrom(
			const TSharedPtr<PCGExClusters::FCluster>& Cluster,
			const TSharedPtr<PCGExData::TBuffer<int64>>& OrbitalMaskReader,
			const TSharedPtr<PCGExData::TBuffer<int64>>& EdgeIndicesReader,
			int32 InMaxOrbitals);

		/**
		 * Build cache from cluster data and raw arrays (for use immediately after writing).
		 * @param Cluster The cluster providing topology (nodes, edges, links)
		 * @param VertexOrbitalMasks Raw array of orbital masks indexed by PointIndex
		 * @param EdgePackedIndices Raw array of packed orbital indices indexed by EdgeIndex
		 * @param InMaxOrbitals Maximum orbital count from OrbitalSet
		 * @return True if cache was built successfully
		 */
		bool BuildFromArrays(
			const TSharedPtr<PCGExClusters::FCluster>& Cluster,
			const TArray<int64>& VertexOrbitalMasks,
			const TArray<int64>& EdgePackedIndices,
			int32 InMaxOrbitals);

		/** Get orbital mask for a node */
		FORCEINLINE int64 GetOrbitalMask(int32 NodeIndex) const
		{
			return NodeOrbitalMasks[NodeIndex];
		}

		/** Get neighbor node index at orbital for a node (-1 if no neighbor) */
		FORCEINLINE int32 GetNeighborAtOrbital(int32 NodeIndex, int32 OrbitalIndex) const
		{
			return FlatOrbitalToNeighbor[NodeIndex * MaxOrbitals + OrbitalIndex];
		}

		/** Check if a node has any orbitals (non-zero mask) */
		FORCEINLINE bool HasOrbitals(int32 NodeIndex) const
		{
			return NodeOrbitalMasks[NodeIndex] != 0;
		}

		/** Get neighbor count for a node */
		FORCEINLINE int32 GetNeighborCount(int32 NodeIndex) const
		{
			int32 Count = 0;
			const int32 BaseIndex = NodeIndex * MaxOrbitals;
			for (int32 i = 0; i < MaxOrbitals; ++i)
			{
				if (FlatOrbitalToNeighbor[BaseIndex + i] >= 0) { ++Count; }
			}
			return Count;
		}

		/**
		 * Initialize valency states array with node indices.
		 * States will have NodeIndex set and ResolvedModule = UNSET.
		 * @param OutStates Array to populate (will be resized)
		 */
		void InitializeStates(TArray<FValencyState>& OutStates) const;

		/** Get node count */
		FORCEINLINE int32 GetNumNodes() const { return NumNodes; }

		/** Get max orbital count */
		FORCEINLINE int32 GetMaxOrbitals() const { return MaxOrbitals; }

		/** Check if cache is valid and ready to use */
		FORCEINLINE bool IsValid() const { return NumNodes > 0 && MaxOrbitals > 0; }

		/** Clear the cache */
		void Reset();

	private:
		int32 NumNodes = 0;
		int32 MaxOrbitals = 0;

		/** Orbital masks per node: [NodeIndex] -> int64 bitmask */
		TArray<int64> NodeOrbitalMasks;

		/**
		 * Flat array for neighbor lookup: [NodeIndex * MaxOrbitals + OrbitalIndex] -> NeighborNodeIndex
		 * Value of -1 indicates no neighbor at that orbital.
		 */
		TArray<int32> FlatOrbitalToNeighbor;
	};
}
