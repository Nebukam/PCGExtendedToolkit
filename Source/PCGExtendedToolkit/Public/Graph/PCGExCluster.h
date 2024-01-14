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
		int32 ClusterID = -1;
		TMap<int32, int32> PointIndexMap; //Cluster vertices
		TArray<FNode> Nodes;
		TArray<PCGExGraph::FIndexedEdge> Edges;
		FBox Bounds;

		PCGExData::FPointIO* PointsIO = nullptr;
		PCGExData::FPointIO* EdgesIO = nullptr;
		FCluster();

		~FCluster();

		bool BuildFrom(const PCGExData::FPointIO& InPoints, const PCGExData::FPointIO& InEdges);
		int32 FindClosestNode(const FVector& Position) const;

		const FNode& GetNodeFromPointIndex(const int32 Index) const;

	protected:
		FNode& GetOrCreateNode(const int32 PointIndex, const TArray<FPCGPoint>& InPoints);
	};
}
