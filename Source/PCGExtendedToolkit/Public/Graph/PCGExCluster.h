// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExEdge.h"
#include "PCGExGraph.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Geometry/PCGExGeo.h"

namespace PCGExCluster
{
	struct FCluster;

	struct PCGEXTENDEDTOOLKIT_API FNode : public PCGExGraph::FNode
	{
		FVector Position;

		TArray<int32> AdjacentNodes;

		FNode()
		{
			PointIndex = -1;
			Position = FVector::ZeroVector;
			AdjacentNodes.Empty();
			Edges.Empty();
		}

		~FNode();

		void AddConnection(const int32 EdgeIndex, const int32 NodeIndex);
		bool GetNormal(FCluster* InCluster, FVector& OutNormal) const;
		int32 GetEdgeIndex(int32 AdjacentNodeIndex) const;
	};

	struct PCGEXTENDEDTOOLKIT_API FCluster
	{
		bool bEdgeLengthsDirty = true;
		bool bValid = false;
		int32 ClusterID = -1;
		TMap<int32, int32> PointIndexMap; // Node index -> Point Index
		TMap<uint64, int32> EdgeIndexMap; // Edge Hash -> Edge Index
		TArray<FNode> Nodes;
		TArray<PCGExGraph::FIndexedEdge> Edges;
		TArray<double> EdgeLengths;
		FBox Bounds;

		PCGExData::FPointIO* PointsIO = nullptr;
		PCGExData::FPointIO* EdgesIO = nullptr;
		FCluster();

		~FCluster();

		bool BuildFrom(
			const PCGExData::FPointIO& EdgeIO,
			const TArray<FPCGPoint>& InNodePoints,
			const TMap<int32, int32>& InNodeIndicesMap,
			const TArray<int32>& PerNodeEdgeNums);

		void BuildPartialFrom(const TArray<FVector>& Positions, const TSet<uint64>& InEdges);

		int32 FindClosestNode(const FVector& Position, const int32 MinNeighbors = 0) const;

		int32 FindClosestNeighbor(const int32 NodeIndex, const FVector& Position, int32 MinNeighborCount = 1) const;
		int32 FindClosestNeighbor(const int32 NodeIndex, const FVector& Position, const TSet<int32>& Exclusion, int32 MinNeighborCount = 1) const;

		const FNode& GetNodeFromPointIndex(const int32 Index) const;
		const PCGExGraph::FIndexedEdge& GetEdgeFromNodeIndices(const int32 A, const int32 B) const;
		void ComputeEdgeLengths(bool bNormalize = false);

		void GetConnectedNodes(const int32 FromIndex, TArray<int32>& OutIndices, const int32 SearchDepth) const;
		
		FVector GetEdgeDirection(const int32 FromIndex, const int32 ToIndex) const;
		FVector GetCentroid(const int32 NodeIndex) const;
		FVector GetCentroid(const int32 NodeIndex, const TSet<int32>& Exclusion) const;
		
		int32 FindClosestNeighborInDirection(const int32 NodeIndex, const FVector& Direction, int32 MinNeighborCount = 1) const;
		int32 FindClosestNeighborInDirection(const int32 NodeIndex, const FVector& Direction, const TSet<int32>& Exclusion, int32 MinNeighborCount = 1) const;
		int32 FindClosestNeighborInDirection(const int32 NodeIndex, const FVector& Direction, const FVector& Guide, int32 MinNeighborCount = 1) const;
		int32 FindClosestNeighborInDirection(const int32 NodeIndex, const FVector& Direction, const FVector& Guide, const TSet<int32>& Exclusion, int32 MinNeighborCount = 1) const;

		int32 FindClosestNeighborLeft(const int32 NodeIndex, const FVector& Direction, const TSet<int32>& Exclusion, const int32 MinNeighbors) const;
		
		void ProjectNodes(const FPCGExGeo2DProjectionSettings& ProjectionSettings);
		
	protected:
		FNode& GetOrCreateNode(const int32 PointIndex, const TArray<FPCGPoint>& InPoints);
	};

	struct PCGEXTENDEDTOOLKIT_API FNodeChain
	{
		int32 First = -1;
		int32 Last = -1;
		TArray<int32> Nodes;
		TArray<int32> Edges;

		FNodeChain()
		{
		}

		~FNodeChain()
		{
			Nodes.Empty();
			Edges.Empty();
		}
	};
}

namespace PCGExClusterTask
{
	class PCGEXTENDEDTOOLKIT_API FBuildCluster : public FPCGExNonAbandonableTask
	{
	public:
		FBuildCluster(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		              PCGExCluster::FCluster* InCluster,
		              PCGExData::FPointIO* InEdgeIO,
		              TMap<int32, int32>* InNodeIndicesMap,
		              TArray<int32>* InPerNodeEdgeNums) :
			FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			Cluster(InCluster),
			EdgeIO(InEdgeIO),
			NodeIndicesMap(InNodeIndicesMap),
			PerNodeEdgeNums(InPerNodeEdgeNums)
		{
		}

		PCGExCluster::FCluster* Cluster = nullptr;
		PCGExData::FPointIO* EdgeIO = nullptr;
		TMap<int32, int32>* NodeIndicesMap = nullptr;
		TArray<int32>* PerNodeEdgeNums = nullptr;

		virtual bool ExecuteTask() override;
	};

	class PCGEXTENDEDTOOLKIT_API FFindNodeChains : public FPCGExNonAbandonableTask
	{
	public:
		FFindNodeChains(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		                PCGExCluster::FCluster* InCluster,
		                TSet<int32>* InFixtures,
		                TArray<PCGExCluster::FNodeChain*>* InChains) :
			FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			Cluster(InCluster),
			Fixtures(InFixtures),
			Chains(InChains)
		{
		}

		PCGExCluster::FCluster* Cluster = nullptr;
		TSet<int32>* Fixtures = nullptr;
		TArray<PCGExCluster::FNodeChain*>* Chains = nullptr;

		virtual bool ExecuteTask() override;
	};
}
