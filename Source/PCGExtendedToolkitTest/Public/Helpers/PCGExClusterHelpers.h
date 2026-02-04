// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Clusters/PCGExNode.h"
#include "Clusters/PCGExEdge.h"
#include "Clusters/PCGExClusterCache.h"
#include "Containers/PCGExIndexLookup.h"

namespace PCGExClusters
{
	class FCluster;
}

namespace PCGExTest
{
	/**
	 * Lightweight test cluster that doesn't require full PCG infrastructure.
	 * Provides the same interface as FCluster for chain testing purposes.
	 */
	class PCGEXTENDEDTOOLKITTEST_API FTestCluster : public TSharedFromThis<FTestCluster>
	{
	public:
		TSharedPtr<PCGEx::FIndexLookup> NodeIndexLookup;
		TSharedPtr<TArray<PCGExClusters::FNode>> Nodes;
		TSharedPtr<TArray<PCGExGraphs::FEdge>> Edges;
		TArray<FVector> Positions;

		PCGExClusters::FNode* NodesDataPtr = nullptr;
		PCGExGraphs::FEdge* EdgesDataPtr = nullptr;

		int32 NumRawVtx = 0;
		int32 NumRawEdges = 0;
		bool bValid = false;

		FBox Bounds = FBox(ForceInit);

		FTestCluster() = default;

		void Initialize(
			const TSharedPtr<PCGEx::FIndexLookup>& InNodeIndexLookup,
			const TSharedPtr<TArray<PCGExClusters::FNode>>& InNodes,
			const TSharedPtr<TArray<PCGExGraphs::FEdge>>& InEdges,
			const TArray<FVector>& InPositions);

		// FCluster-compatible interface for chain testing
		FORCEINLINE PCGExClusters::FNode* GetNode(const int32 Index) const { return (NodesDataPtr + Index); }
		FORCEINLINE PCGExClusters::FNode* GetNode(const PCGExGraphs::FLink Lk) const { return (NodesDataPtr + Lk.Node); }
		FORCEINLINE int32 GetNodePointIndex(const int32 Index) const { return (NodesDataPtr + Index)->PointIndex; }
		FORCEINLINE int32 GetNodePointIndex(const PCGExGraphs::FLink Lk) const { return (NodesDataPtr + Lk.Node)->PointIndex; }
		FORCEINLINE PCGExGraphs::FEdge* GetEdge(const int32 Index) const { return (EdgesDataPtr + Index); }
		FORCEINLINE PCGExGraphs::FEdge* GetEdge(const PCGExGraphs::FLink Lk) const { return (EdgesDataPtr + Lk.Edge); }

		FORCEINLINE PCGExClusters::FNode* GetEdgeOtherNode(const PCGExGraphs::FLink Lk) const
		{
			return (NodesDataPtr + NodeIndexLookup->Get((EdgesDataPtr + Lk.Edge)->Other((NodesDataPtr + Lk.Node)->PointIndex)));
		}

		FORCEINLINE FVector GetPos(const int32 NodeIndex) const
		{
			const int32 PointIndex = GetNodePointIndex(NodeIndex);
			return Positions.IsValidIndex(PointIndex) ? Positions[PointIndex] : FVector::ZeroVector;
		}

		FORCEINLINE FVector GetPos(const PCGExClusters::FNode* Node) const
		{
			return Positions.IsValidIndex(Node->PointIndex) ? Positions[Node->PointIndex] : FVector::ZeroVector;
		}

		FORCEINLINE FVector GetDir(const int32 FromNode, const int32 ToNode) const
		{
			return (GetPos(ToNode) - GetPos(FromNode)).GetSafeNormal();
		}

		// Cache support (matches FCluster interface)
		template <typename T>
		TSharedPtr<T> GetCachedData(FName Key, uint32 ExpectedContextHash = 0) const
		{
			FReadScopeLock ReadLock(ClusterLock);
			if (const TSharedPtr<PCGExClusters::ICachedClusterData>* Entry = CachedData.Find(Key))
			{
				if (ExpectedContextHash == 0 || (*Entry)->ContextHash == ExpectedContextHash)
				{
					return StaticCastSharedPtr<T>(*Entry);
				}
			}
			return nullptr;
		}

		void SetCachedData(FName Key, const TSharedPtr<PCGExClusters::ICachedClusterData>& Data);
		void ClearCachedData();

	private:
		mutable FRWLock ClusterLock;
		TMap<FName, TSharedPtr<PCGExClusters::ICachedClusterData>> CachedData;
	};

	/**
	 * Builder pattern for creating test clusters with specific topologies.
	 *
	 * Example Usage:
	 * @code
	 * // Create a simple linear chain: 0-1-2-3-4
	 * TSharedRef<FTestCluster> Cluster = FClusterBuilder()
	 *     .WithLinearChain(5)
	 *     .Build();
	 *
	 * // Create a chain with branches:
	 * //     1
	 * //    /
	 * // 0-2-3-4
	 * //    \
	 * //     5
	 * TSharedRef<FTestCluster> Cluster = FClusterBuilder()
	 *     .AddNode(0, FVector(0, 0, 0))
	 *     .AddNode(1, FVector(100, 100, 0))
	 *     .AddNode(2, FVector(100, 0, 0))
	 *     .AddNode(3, FVector(200, 0, 0))
	 *     .AddNode(4, FVector(300, 0, 0))
	 *     .AddNode(5, FVector(100, -100, 0))
	 *     .AddEdge(0, 2)  // 0-2
	 *     .AddEdge(2, 1)  // 2-1
	 *     .AddEdge(2, 3)  // 2-3
	 *     .AddEdge(2, 5)  // 2-5
	 *     .AddEdge(3, 4)  // 3-4
	 *     .Build();
	 * @endcode
	 */
	class PCGEXTENDEDTOOLKITTEST_API FClusterBuilder
	{
	public:
		FClusterBuilder();
		~FClusterBuilder();

		/**
		 * Add a node at a specific position
		 * @param PointIndex The point index for this node
		 * @param Position World position of the node
		 */
		FClusterBuilder& AddNode(int32 PointIndex, const FVector& Position);

		/**
		 * Add an edge between two nodes (by point index)
		 * @param StartPointIndex Point index of start node
		 * @param EndPointIndex Point index of end node
		 */
		FClusterBuilder& AddEdge(int32 StartPointIndex, int32 EndPointIndex);

		/**
		 * Create a simple linear chain: 0-1-2-...-N
		 * @param NumNodes Number of nodes in the chain
		 * @param Spacing Distance between consecutive nodes
		 * @param Origin Starting position
		 */
		FClusterBuilder& WithLinearChain(int32 NumNodes, double Spacing = 100.0, const FVector& Origin = FVector::ZeroVector);

		/**
		 * Create a closed loop: 0-1-2-...-N-0
		 * @param NumNodes Number of nodes in the loop
		 * @param Radius Radius of the circular loop
		 * @param Center Center position
		 */
		FClusterBuilder& WithClosedLoop(int32 NumNodes, double Radius = 100.0, const FVector& Center = FVector::ZeroVector);

		/**
		 * Create a star topology: Center node connected to N leaf nodes
		 * @param NumLeaves Number of leaf nodes
		 * @param Radius Distance from center to leaves
		 * @param Center Center position
		 */
		FClusterBuilder& WithStar(int32 NumLeaves, double Radius = 100.0, const FVector& Center = FVector::ZeroVector);

		/**
		 * Create a grid topology with specified dimensions
		 * @param CountX Nodes in X direction
		 * @param CountY Nodes in Y direction
		 * @param Spacing Distance between nodes
		 * @param Origin Starting corner position
		 */
		FClusterBuilder& WithGrid(int32 CountX, int32 CountY, double Spacing = 100.0, const FVector& Origin = FVector::ZeroVector);

		/**
		 * Build the cluster
		 * @return Shared ref to the built cluster
		 */
		TSharedRef<FTestCluster> Build();

		/**
		 * Get the positions array (for verification)
		 */
		const TArray<FVector>& GetPositions() const { return Positions; }

	private:
		TArray<FVector> Positions;
		TArray<TPair<int32, int32>> EdgeDefinitions;
		TMap<int32, int32> PointToNodeIndex;
	};

	/**
	 * Utility functions for verifying cluster state
	 */
	namespace ClusterVerify
	{
		/** Verify node count */
		PCGEXTENDEDTOOLKITTEST_API bool HasNodeCount(const TSharedRef<FTestCluster>& Cluster, int32 ExpectedCount);

		/** Verify edge count */
		PCGEXTENDEDTOOLKITTEST_API bool HasEdgeCount(const TSharedRef<FTestCluster>& Cluster, int32 ExpectedCount);

		/** Verify a node has expected neighbor count */
		PCGEXTENDEDTOOLKITTEST_API bool NodeHasNeighborCount(const TSharedRef<FTestCluster>& Cluster, int32 NodeIndex, int32 ExpectedNeighbors);

		/** Verify a node is a leaf (1 neighbor) */
		PCGEXTENDEDTOOLKITTEST_API bool NodeIsLeaf(const TSharedRef<FTestCluster>& Cluster, int32 NodeIndex);

		/** Verify a node is binary (2 neighbors) */
		PCGEXTENDEDTOOLKITTEST_API bool NodeIsBinary(const TSharedRef<FTestCluster>& Cluster, int32 NodeIndex);

		/** Verify a node is complex (3+ neighbors) */
		PCGEXTENDEDTOOLKITTEST_API bool NodeIsComplex(const TSharedRef<FTestCluster>& Cluster, int32 NodeIndex);

		/** Count nodes with specific neighbor count */
		PCGEXTENDEDTOOLKITTEST_API int32 CountNodesWithNeighbors(const TSharedRef<FTestCluster>& Cluster, int32 NeighborCount);

		/** Count leaf nodes */
		PCGEXTENDEDTOOLKITTEST_API int32 CountLeafNodes(const TSharedRef<FTestCluster>& Cluster);

		/** Count binary nodes */
		PCGEXTENDEDTOOLKITTEST_API int32 CountBinaryNodes(const TSharedRef<FTestCluster>& Cluster);

		/** Count complex nodes */
		PCGEXTENDEDTOOLKITTEST_API int32 CountComplexNodes(const TSharedRef<FTestCluster>& Cluster);
	}
}
