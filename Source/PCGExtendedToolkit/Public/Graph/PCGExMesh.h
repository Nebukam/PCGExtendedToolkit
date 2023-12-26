// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExEdge.h"

namespace PCGExMesh
{
	struct PCGEXTENDEDTOOLKIT_API FIndexedEdge : public PCGExGraph::FUnsignedEdge
	{
		int32 Index = -1;

		FIndexedEdge(const int32 InIndex, const int32 InStart, const int32 InEnd)
			: FUnsignedEdge(InStart, InEnd, EPCGExEdgeType::Complete),
			  Index(InIndex)
		{
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FVertex
	{
		int32 MeshIndex = -1;
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
	};

	struct PCGEXTENDEDTOOLKIT_API FTetrahedron
	{
		FVertex* Vtx[4];
		FSphere Circumsphere;
		bool bValid = false;

		FTetrahedron(FVertex* InVtx1, FVertex* InVtx2, FVertex* InVtx3, FVertex* InVtx4);

		bool SharedFace(const FTetrahedron* OtherTetrahedron, FVertex& A, FVertex& B, FVertex& C) const;
		void RegisterEdges(TSet<uint64>& UniqueEdges, TArray<PCGExGraph::FUnsignedEdge>& Edges, const int32 MaxIndex) const;
		void Draw(const UWorld* World) const;
	};

	struct PCGEXTENDEDTOOLKIT_API FDelaunayTriangulation
	{
		mutable FRWLock TetraLock;
		int32 MeshID = -1;
		int32 NumPoints = 0;
		TArray<FVertex> Vertices;
		TArray<PCGExGraph::FUnsignedEdge> Edges;
		TMap<uint64, FTetrahedron*> Tetrahedrons;
		uint64 TUID = 0;

		FDelaunayTriangulation();

		~FDelaunayTriangulation();

		bool PrepareFrom(const PCGExData::FPointIO& PointIO);
		FTetrahedron* EmplaceTetrahedron(FVertex* InVtx1, FVertex* InVtx2, FVertex* InVtx3, FVertex* InVtx4);

		void InsertVertex(int32 Index);

		void FindEdges();
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

	struct PCGEXTENDEDTOOLKIT_API FMesh
	{
		int32 MeshID = -1;
		TMap<int32, int32> IndicesMap; //Mesh vertices
		TArray<FVertex> Vertices;
		TArray<FIndexedEdge> Edges;
		FBox Bounds;

		PCGExData::FPointIO* PointsIO = nullptr;
		PCGExData::FPointIO* EdgesIO = nullptr;
		FMesh();

		~FMesh();

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
