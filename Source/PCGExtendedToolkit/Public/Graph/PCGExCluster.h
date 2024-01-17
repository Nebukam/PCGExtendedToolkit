// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExEdge.h"
#include "PCGExGraph.h"
#include "Data/PCGExAttributeHelpers.h"

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
		bool GetNormal(FCluster* InCluster, FVector& OutNormal);
	};

	struct PCGEXTENDEDTOOLKIT_API FScoredNode
	{
		FScoredNode(const FNode& InNode, const double InWeight)
			: Node(&InNode),
			  Score(InWeight)
		{
		}

		FScoredNode(const FNode& InNode, const double InScore, FScoredNode* InFrom)
			: Node(&InNode),
			  Score(InScore),
			  From(InFrom)
		{
		}

		const FNode* Node = nullptr;
		double Score = -1;
		FScoredNode* From = nullptr;
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
			const PCGExData::FPointIO& InEdges,
			const TArray<FPCGPoint>& InNodePoints,
			const TMap<int32, int32>& InNodeIndicesMap,
			const TArray<int32>& PerNodeEdgeNums);
		int32 FindClosestNode(const FVector& Position) const;

		const FNode& GetNodeFromPointIndex(const int32 Index) const;
		const PCGExGraph::FIndexedEdge& GetEdgeFromNodeIndices(const int32 A, const int32 B) const;
		void ComputeEdgeLengths(bool bNormalize = false);

	protected:
		FNode& GetOrCreateNode(const int32 PointIndex, const TArray<FPCGPoint>& InPoints);
	};
}

class PCGEXTENDEDTOOLKIT_API FPCGExBuildCluster : public FPCGExNonAbandonableTask
{
public:
	FPCGExBuildCluster(
		FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		PCGExCluster::FCluster* InCluster, PCGExData::FPointIO* InEdgeIO, TMap<int32, int32>* InNodeIndicesMap, TArray<int32>* InPerNodeEdgeNums) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		Cluster(InCluster), EdgeIO(InEdgeIO), NodeIndicesMap(InNodeIndicesMap), PerNodeEdgeNums(InPerNodeEdgeNums)
	{
	}

	PCGExCluster::FCluster* Cluster = nullptr;
	PCGExData::FPointIO* EdgeIO = nullptr;
	TMap<int32, int32>* NodeIndicesMap = nullptr;
	TArray<int32>* PerNodeEdgeNums = nullptr;

	virtual bool ExecuteTask() override;
};
