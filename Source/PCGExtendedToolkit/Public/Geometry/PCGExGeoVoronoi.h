// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGeo.h"
#include "PCGExGeoDelaunay.h"
#include "PCGExGeoPrimtives.h"
#include "Graph/PCGExGraph.h"

namespace PCGExGeo
{
	template <int DIMENSIONS>
	class PCGEXTENDEDTOOLKIT_API TVoronoiEdge
	{
	public:
		TDelaunayCell<DIMENSIONS>* From = nullptr;
		TDelaunayCell<DIMENSIONS>* To = nullptr;

		TVoronoiEdge(
			TDelaunayCell<DIMENSIONS>* InFrom,
			TDelaunayCell<DIMENSIONS>* InTo)
		{
			From = InFrom;
			To = InTo;
		}

		~TVoronoiEdge()
		{
			From = nullptr;
			To = nullptr;
		}

		bool operator==(const TVoronoiEdge& Other) const
		{
			if (*Other == this) { return true; }
			return From == Other.To; //TODO: Double check
		}

		bool operator!=(const TVoronoiEdge& Other) const
		{
			return !(this == Other);
		}

		uint64 GetUnsignedHash() const { return PCGExGraph::GetUnsignedHash64(From->Circumcenter->Id, To->Circumcenter->Id); }
		PCGExGraph::FUnsignedEdge GetUnsignedEdge() const { return PCGExGraph::FUnsignedEdge(From->Circumcenter->Id, To->Circumcenter->Id); }
	};

	template <int DIMENSIONS>
	class PCGEXTENDEDTOOLKIT_API TVoronoiRegion
	{
	public:
		int32 Id = -1;
		TArray<TDelaunayCell<DIMENSIONS>*> Cells;
		TArray<TVoronoiEdge<DIMENSIONS>*> Edges;
		bool bIsWithinBounds = true;

		TVoronoiRegion()
		{
		}

		~TVoronoiRegion()
		{
			Cells.Empty();
			PCGEX_DELETE_TARRAY(Edges) // Region owns edges
		}

		void CheckWithinBounds()
		{
			bIsWithinBounds = true;
			for (TDelaunayCell<DIMENSIONS>* Cell : Cells)
			{
				if (!Cell->bIsWithinBounds)
				{
					bIsWithinBounds = false;
					break;
				}
			}
		}
	};

	template <int DIMENSIONS, typename T_DELAUNAY>
	class PCGEXTENDEDTOOLKIT_API TVoronoiMesh
	{
		bool bOwnsVertices = true;

	public:
		TArray<TVoronoiRegion<DIMENSIONS>*> Regions;
		T_DELAUNAY* Delaunay = nullptr;
		double BoundsExtension = 0;
		EPCGExCellCenter CellCenter = EPCGExCellCenter::Circumcenter;

		TVoronoiMesh()
		{
		}

		~TVoronoiMesh()
		{
			PCGEX_DELETE_TARRAY(Regions)
			PCGEX_DELETE(Delaunay)
		}

		bool PrepareFrom(const TArray<FPCGPoint>& InPoints)
		{
			PCGEX_DELETE_TARRAY(Regions)
			PCGEX_DELETE(Delaunay)

			Delaunay = new T_DELAUNAY();
			Delaunay->BoundsExtension = BoundsExtension;
			Delaunay->CellCenter = CellCenter;

			if (!Delaunay->PrepareFrom(InPoints)) { return false; }

			return true;
		}

		bool PrepareFrom(const TArray<TFVtx<DIMENSIONS>*>& InVertices)
		{
			PCGEX_DELETE_TARRAY(Regions)
			PCGEX_DELETE(Delaunay)

			Delaunay = new T_DELAUNAY();
			Delaunay->BoundsExtension = BoundsExtension;
			Delaunay->CellCenter = CellCenter;

			if (!Delaunay->PrepareFrom(InVertices)) { return false; }

			return true;
		}

		void Generate()
		{
			Delaunay->Generate();

			for (int i = 0; i < Delaunay->Vertices.Num(); i++) { Delaunay->Vertices[i]->Tag = i; }

			int32 CellIndex = 0;
			for (TDelaunayCell<DIMENSIONS>* Cell : Delaunay->Cells)
			{
				Cell->Circumcenter->Id = CellIndex;
				Cell->Simplex->Tag = CellIndex;
				CellIndex++;
			}

			TArray<TDelaunayCell<DIMENSIONS>*> LocalCells;
			LocalCells.Reserve(Delaunay->Cells.Num());

			TMap<int32, TDelaunayCell<DIMENSIONS>*> NeighbourCells;

			for (TFVtx<DIMENSIONS>* Vertex : Delaunay->Vertices)
			{
				LocalCells.Reset();

				for (TDelaunayCell<DIMENSIONS>* Cell : Delaunay->Cells)
				{
					for (TFVtx<DIMENSIONS>* SVertex : Cell->Simplex->Vertices)
					{
						if (SVertex->Tag == Vertex->Tag)
						{
							LocalCells.Add(Cell);
							break;
						}
					}
				}

				if (!LocalCells.IsEmpty())
				{
					TVoronoiRegion<DIMENSIONS>* Region = new TVoronoiRegion<DIMENSIONS>();
					Region->Cells.Append(LocalCells);

					NeighbourCells.Empty();

					for (TDelaunayCell<DIMENSIONS>* Cell : Delaunay->Cells)
					{
						NeighbourCells.Add(Cell->Circumcenter->Id, Cell);
					}

					for (TDelaunayCell<DIMENSIONS>* Cell : Delaunay->Cells)
					{
						TFSimplex<DIMENSIONS>* Simplex = Cell->Simplex;

						for (int k = 0; k < DIMENSIONS; k++)
						{
							if (!Simplex->AdjacentFaces[k]) { continue; }

							if (int32 Key = Simplex->AdjacentFaces[k]->Tag;
								NeighbourCells.Contains(Key))
							{
								Region->Edges.Add(new TVoronoiEdge<DIMENSIONS>(Cell, *NeighbourCells.Find(Key)));
							}
						}
					}
					Region->CheckWithinBounds();
					Region->Id = Regions.Num();
					Regions.Add(Region);
				}
			}
		}

		void GetUniqueEdges(TArray<PCGExGraph::FUnsignedEdge>& OutEdges, bool bPruneOutsideBounds = false)
		{
			TSet<uint64> UniqueEdges;
			UniqueEdges.Reserve(Regions.Num() * 5);

			for (const TVoronoiRegion<DIMENSIONS>* Region : Regions)
			{
				for (const TVoronoiEdge<DIMENSIONS>* Edge : Region->Edges)
				{
					if (bPruneOutsideBounds && (!Edge->From->bIsWithinBounds || !Edge->To->bIsWithinBounds)) { continue; }
					if (const uint64 Hash = Edge->GetUnsignedHash();
						!UniqueEdges.Contains(Hash))
					{
						OutEdges.Add(Edge->GetUnsignedEdge());
						UniqueEdges.Add(Hash);
					}
				}
			}

			UniqueEdges.Empty();
		}
	};

	class PCGEXTENDEDTOOLKIT_API TVoronoiMesh2 : public TVoronoiMesh<3, TDelaunayTriangulation2>
	{
	public:
		TVoronoiMesh2() : TVoronoiMesh<3, TDelaunayTriangulation2>()
		{
		}
	};

	class PCGEXTENDEDTOOLKIT_API TVoronoiMesh3 : public TVoronoiMesh<4, TDelaunayTriangulation3>
	{
	public:
		TVoronoiMesh3() : TVoronoiMesh<4, TDelaunayTriangulation3>()
		{
		}
	};
}
