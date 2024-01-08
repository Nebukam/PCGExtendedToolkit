// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExEdge.h"

namespace PCGExCluster
{
	struct PCGEXTENDEDTOOLKIT_API FIndexedEdge : public PCGExGraph::FUnsignedEdge
	{
		int32 Index = -1;

		FIndexedEdge(const int32 InIndex, const int32 InStart, const int32 InEnd)
			: FUnsignedEdge(InStart, InEnd),
			  Index(InIndex)
		{
		}
	};

	struct FCluster;
	
	struct PCGEXTENDEDTOOLKIT_API FVertex
	{
		int32 ClusterIndex = -1;
		int32 PointIndex = -1;
		FVector Position;
		TArray<int32> Neighbors;
		TArray<int32> Edges;

		FVertex()
		{
			PointIndex = -1;
			Position = FVector::ZeroVector;
			Neighbors.Empty();
			Edges.Empty();
		}

		~FVertex();

		void AddNeighbor(const int32 EdgeIndex, const int32 VertexIndex);
		bool GetNormal(FCluster* InCluster, FVector& OutNormal);
	};
	
	struct PCGEXTENDEDTOOLKIT_API FScoredVertex
	{
		FScoredVertex(const FVertex& InVertex, const double InWeight)
			: Vertex(&InVertex),
			  Score(InWeight)
		{
		}

		FScoredVertex(const FVertex& InVertex, const double InScore, FScoredVertex* InFrom)
			: Vertex(&InVertex),
			  Score(InScore),
			  From(InFrom)
		{
		}

		const FVertex* Vertex = nullptr;
		double Score = -1;
		FScoredVertex* From = nullptr;
	};

	struct PCGEXTENDEDTOOLKIT_API FCluster
	{
		int32 ClusterID = -1;
		TMap<int32, int32> IndicesMap; //Cluster vertices
		TArray<FVertex> Vertices;
		TArray<FIndexedEdge> Edges;
		FBox Bounds;

		PCGExData::FPointIO* PointsIO = nullptr;
		PCGExData::FPointIO* EdgesIO = nullptr;
		FCluster();

		~FCluster();

		void BuildFrom(const PCGExData::FPointIO& InPoints, const PCGExData::FPointIO& InEdges);
		int32 FindClosestVertex(const FVector& Position) const;

		const FVertex& GetVertexFromPointIndex(const int32 Index) const;
		const FVertex& GetVertex(const int32 Index) const;

		bool HasInvalidEdges() const { return bHasInvalidEdges; }

	protected:
		bool bHasInvalidEdges = false;
		FVertex& GetOrCreateVertex(const int32 PointIndex, bool& bJustCreated);
	};
}
