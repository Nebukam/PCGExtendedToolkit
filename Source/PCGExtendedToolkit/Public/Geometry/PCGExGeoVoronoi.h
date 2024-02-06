// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGeo.h"
#include "PCGExGeoDelaunay.h"
#include "PCGExGeoPrimtives.h"
#include "Graph/PCGExGraph.h"

namespace PCGExGeo
{
	class FVoronoiRegionTask;

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

		uint64 H64U() const { return PCGEx::H64U(From->Circumcenter->Id, To->Circumcenter->Id); }
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
		bool bIsOnHull = false;

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
		FRWLock AsyncLock;

	public:
		TArray<TVoronoiRegion<DIMENSIONS>*> Regions;
		T_DELAUNAY* Delaunay = nullptr;
		double BoundsExtension = 0;
		EPCGExCellCenter CellCenter = EPCGExCellCenter::Circumcenter;

		TVoronoiMesh()
		{
		}

		virtual ~TVoronoiMesh()
		{
			PCGEX_DELETE_TARRAY(Regions)
			PCGEX_DELETE(Delaunay)
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
					if (const uint64 Hash = Edge->H64U();
						!UniqueEdges.Contains(Hash))
					{
						OutEdges.Add(Edge->GetUnsignedEdge());
						UniqueEdges.Add(Hash);
					}
				}
			}

			UniqueEdges.Empty();
		}

		virtual void GetVoronoiPoints(TArray<FPCGPoint>& OutPoints, const EPCGExCellCenter Method)
		{
			OutPoints.SetNum(Delaunay->Cells.Num());

			switch (Method)
			{
			default:
			case EPCGExCellCenter::Balanced:
				for (const TDelaunayCell<DIMENSIONS>* Cell : Delaunay->Cells)
				{
					FPCGPoint& Point = OutPoints[Cell->Circumcenter->Id];
					Point.Transform.SetLocation(Cell->GetBestCenter());
					PCGExMath::RandomizeSeed(Point);
				}
				break;
			case EPCGExCellCenter::Circumcenter:
				for (const TDelaunayCell<DIMENSIONS>* Cell : Delaunay->Cells)
				{
					FPCGPoint& Point = OutPoints[Cell->Circumcenter->Id];
					Point.Transform.SetLocation(PCGExGeo::GetV3(Cell->Circumcenter));
					PCGExMath::RandomizeSeed(Point);
				}
				break;
			case EPCGExCellCenter::Centroid:
				for (const TDelaunayCell<DIMENSIONS>* Cell : Delaunay->Cells)
				{
					FPCGPoint& Point = OutPoints[Cell->Circumcenter->Id];
					Point.Transform.SetLocation(Cell->Centroid);
					PCGExMath::RandomizeSeed(Point);
				}
				break;
			}
		}

	protected:
		void InternalPrepare()
		{
			PCGEX_DELETE_TARRAY(Regions)
			PCGEX_DELETE(Delaunay)

			Delaunay = new T_DELAUNAY();
			Delaunay->BoundsExtension = BoundsExtension;
			Delaunay->CellCenter = CellCenter;
		}

	public:
		bool PrepareFrom(const TArray<FPCGPoint>& InPoints)
		{
			InternalPrepare();
			return Delaunay->PrepareFrom(InPoints);
		}

		bool PrepareFrom(const TArray<TFVtx<DIMENSIONS>*>& InVertices)
		{
			InternalPrepare();
			return Delaunay->PrepareFrom(InVertices);
		}

		void Generate()
		{
			Delaunay->Generate();
			PrepareVoronoi();
			for (int i = 0; i < Delaunay->Vertices.Num(); i++) { ProcessVertex(i); }
		}

		void PrepareVoronoi()
		{
			for (int i = 0; i < Delaunay->Vertices.Num(); i++) { Delaunay->Vertices[i]->Tag = i; }

			int32 CellIndex = 0;
			for (TDelaunayCell<DIMENSIONS>* Cell : Delaunay->Cells)
			{
				Cell->Circumcenter->Id = CellIndex;
				Cell->Simplex->Tag = CellIndex++;
			}
		}

	public:
		void ProcessVertex(int32 Index)
		{
			TArray<TDelaunayCell<DIMENSIONS>*> RegionCells;
			RegionCells.Reserve(20);

			TFVtx<DIMENSIONS>* Vertex = Delaunay->Vertices[Index];

			for (TDelaunayCell<DIMENSIONS>* Cell : Delaunay->Cells)
			{
				for (TFVtx<DIMENSIONS>* SVertex : Cell->Simplex->Vertices)
				{
					if (SVertex->Tag == Vertex->Tag)
					{
						RegionCells.Add(Cell);
						break;
					}
				}
			}

			if (!RegionCells.IsEmpty())
			{
				TMap<int32, TDelaunayCell<DIMENSIONS>*> NeighbourCells;
				TVoronoiRegion<DIMENSIONS>* Region = new TVoronoiRegion<DIMENSIONS>();
				Region->Cells.Append(RegionCells);

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

				{
					FWriteScopeLock WriteLock(AsyncLock);
					Regions.Add(Region);
				}

				NeighbourCells.Empty();
			}
		}

		virtual void StartAsyncPreprocessing(FPCGExAsyncManager* Manager) = 0;
	};

#define PCGEX_DELAUNAY_CLASS(_NUM, _DIM)\
class FPCGExVoronoiRegion##_NUM##Task;\
class PCGEXTENDEDTOOLKIT_API TVoronoiMesh##_NUM : public TVoronoiMesh<_DIM, TDelaunayTriangulation##_NUM>	{	public:\
TVoronoiMesh##_NUM() : TVoronoiMesh<_DIM, TDelaunayTriangulation##_NUM>(){}\
virtual void StartAsyncPreprocessing(FPCGExAsyncManager* Manager){ for (int i = 0; i < Delaunay->Vertices.Num(); i++) { Manager->Start<FPCGExVoronoiRegion##_NUM##Task>(i, nullptr, this); }}};\
class PCGEXTENDEDTOOLKIT_API FPCGExVoronoiRegion##_NUM##Task : public FPCGExNonAbandonableTask{	public:\
FPCGExVoronoiRegion##_NUM##Task(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO, TVoronoiMesh##_NUM* InVoronoiMesh) : FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO), Voronoi(InVoronoiMesh){}\
TVoronoiMesh##_NUM* Voronoi = nullptr;\
virtual bool ExecuteTask() override{ Voronoi->ProcessVertex(TaskIndex); return true; }};

	PCGEX_DELAUNAY_CLASS(2, 3)

	PCGEX_DELAUNAY_CLASS(3, 4)
}
