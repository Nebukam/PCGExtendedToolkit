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

	struct PCGEXTENDEDTOOLKIT_API FDelaunayVertex
	{
		int32 MeshIndex = -1;
		int32 PointIndex = -1;
		FVector Position;
		double Dist = -1;
		int Tag = 0;

		FDelaunayVertex()
		{
			PointIndex = -1;
			Position = FVector::ZeroVector;
		}

		~FDelaunayVertex()
		{
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FSimplex
	{
		FDelaunayVertex* Vtx[4];
		FSimplex* Adjacent[4];
		FSphere Circumsphere;
		FVector Normal;
		FVector Centroid;
		float Offset;
		int Tag;
		bool bIsNormalFlipped = false;
		bool bValid = false;

		FSimplex(FDelaunayVertex* InVtx1, FDelaunayVertex* InVtx2, FDelaunayVertex* InVtx3, FDelaunayVertex* InVtx4);

		double Dot(FDelaunayVertex* OtherVtx)
		{
			if (!OtherVtx) { return 0; }
			double dp = 0;
			for (int i = 0; i < 3; i++) { dp += Normal[i] * OtherVtx->Position[i]; }
			return dp;
		}

		bool Remove(FSimplex* OtherSimplex)
		{
			if (!OtherSimplex) return false;
			for (int i = 0; i < 4; i++)
			{
				if (Adjacent[i] == OtherSimplex)
				{
					Adjacent[i] = nullptr;
					return true;
				}
			}
			return false;
		}

		void CalculateNormal()
		{
			Normal = FVector::CrossProduct(Vtx[1]->Position - Vtx[0]->Position, Vtx[2]->Position - Vtx[1]->Position).GetSafeNormal();
		}

		void CalculateCentroid() { Centroid = (Vtx[0]->Position + Vtx[1]->Position + Vtx[2]->Position) / 3; }

		void UpdateAdjacency(FSimplex* OtherSimplex)
		{
			int i;
			for (i = 0; i < 3; i++)
			{
				Vtx[i]->Tag = 0;               // reset marks on the 1st face
				OtherSimplex->Vtx[i]->Tag = 1; // mark all vertices on the 2nd face
			}

			// find the 1st false index
			for (i = 0; i < 3; i++) { if (Vtx[i]->Tag == 0) { break; } }

			// no vertex was marked
			if (i == 3) return;

			// check if only 1 vertex wasn't marked
			for (int j = i + 1; j < 3; j++) { if (Vtx[j]->Tag == 0) { return; } }

			// if we are here, the two faces share an edge
			Adjacent[i] = OtherSimplex;

			// update the adj. face on the other face - find the vertex that remains marked
			for (i = 0; i < 3; i++) { Vtx[i]->Tag = 0; }

			for (i = 0; i < 3; i++)
			{
				if (OtherSimplex->Vtx[i]->Tag == 1)
				{
					OtherSimplex->Adjacent[i] = this;
					break;
				}
			}
		}

		bool HasNullAdjacency() const
		{
			for (int i = 0; i < 4; i++) { if (!Adjacent[i]) return true; }
			return false;
		}

		bool HasAdjacency() const
		{
			for (int i = 0; i < 4; i++) { if (Adjacent[i]) return true; }
			return false;
		}

		// Function to check if a point is inside a tetrahedron
		bool IsInside(const FVector& Point) const;
		void RegisterEdges(TSet<uint64>& UniqueEdges, TArray<PCGExGraph::FUnsignedEdge>& Edges) const;
		void Draw(const UWorld* World) const;
	};

	struct PCGEXTENDEDTOOLKIT_API FDelaunayTriangulation
	{
		mutable FRWLock TetraLock;
		int32 MeshID = -1;
		TArray<FDelaunayVertex> Vertices;
		TArray<PCGExGraph::FUnsignedEdge> Edges;
		TMap<uint64, FSimplex*> Simplices;
		uint64 TUID = 0;
		int32 CurrentIndex = -1;

		FDelaunayTriangulation();

		~FDelaunayTriangulation();

		bool PrepareFrom(const PCGExData::FPointIO& PointIO);
		FSimplex* EmplaceTetrahedron(FDelaunayVertex* InVtx1, FDelaunayVertex* InVtx2, FDelaunayVertex* InVtx3, FDelaunayVertex* InVtx4);
		void InsertVertex(int32 InIndex);
		void SplitTetrahedron(FDelaunayVertex* Splitter, uint64 Key);
		bool FindNextBadTetrahedronKey(const FVector& Position, uint64& OutKey);
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
