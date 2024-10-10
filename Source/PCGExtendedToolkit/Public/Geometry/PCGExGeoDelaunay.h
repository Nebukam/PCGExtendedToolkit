// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGeo.h"
#include "CompGeom/Delaunay2.h"
#include "CompGeom/Delaunay3.h"

namespace PCGExGeo
{
	struct FDelaunaySite2
	{
		int32 Vtx[3];
		int32 Neighbors[3];
		int32 Id = -1;
		bool bOnHull = false;

		FDelaunaySite2(const UE::Geometry::FIndex3i& InVtx, const UE::Geometry::FIndex3i& InAdjacency, const int32 InId = -1) : Id(InId)
		{
			for (int i = 0; i < 3; i++)
			{
				Vtx[i] = InVtx[i];
				Neighbors[i] = InAdjacency[i];
			}
		}

		FORCEINLINE bool ContainsEdge(const uint64 Edge) const
		{
			return
				Edge == PCGEx::H64U(Vtx[0], Vtx[1]) ||
				Edge == PCGEx::H64U(Vtx[0], Vtx[2]) ||
				Edge == PCGEx::H64U(Vtx[1], Vtx[2]);
		}

		FORCEINLINE uint64 GetSharedEdge(const FDelaunaySite2* Other) const
		{
			return
				Other->ContainsEdge(PCGEx::H64U(Vtx[0], Vtx[1])) ? PCGEx::H64U(Vtx[0], Vtx[1]) :
					Other->ContainsEdge(PCGEx::H64U(Vtx[0], Vtx[2])) ? PCGEx::H64U(Vtx[0], Vtx[2]) :
					PCGEx::H64U(Vtx[1], Vtx[2]);
		}
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ TDelaunay2
	{
	public:
		TArray<FDelaunaySite2> Sites;

		TSet<uint64> DelaunayEdges;
		TSet<int32> DelaunayHull;
		bool IsValid = false;

		mutable FRWLock ProcessLock;

		TDelaunay2()
		{
		}

		~TDelaunay2()
		{
			Clear();
		}

		void Clear()
		{
			Sites.Empty();
			DelaunayEdges.Empty();
			DelaunayHull.Empty();

			IsValid = false;
		}

		bool Process(const TArrayView<FVector>& Positions, const FPCGExGeo2DProjectionDetails& ProjectionDetails)
		{
			Clear();

			if (const int32 NumPositions = Positions.Num(); Positions.IsEmpty() || NumPositions <= 2) { return false; }

			TArray<FVector2D> Positions2D;
			ProjectionDetails.Project(Positions, Positions2D);

			TArray<UE::Geometry::FIndex3i> Triangles;
			TArray<UE::Geometry::FIndex3i> Adjacencies;

			{
				UE::Geometry::FDelaunay2 Triangulation;
				TRACE_CPUPROFILER_EVENT_SCOPE(Delaunay2D::Triangulate);

				if (!Triangulation.Triangulate(Positions2D))
				{
					Positions2D.Empty();
					Clear();
					return false;
				}

				Positions2D.Empty();
				IsValid = true;
				Triangulation.GetTrianglesAndAdjacency(Triangles, Adjacencies);
			}

			const int32 NumSites = Triangles.Num();
			const int32 NumReserve = NumSites * 3;

			DelaunayEdges.Reserve(NumReserve);

			PCGEx::InitArray(Sites, NumSites);

			for (int i = 0; i < NumSites; i++)
			{
				FDelaunaySite2& Site = Sites[i] = FDelaunaySite2(Triangles[i], Adjacencies[i], i);

				for (int a = 0; a < 3; a++)
				{
					for (int b = a + 1; b < 3; b++)
					{
						const uint64 H = PCGEx::H64U(Site.Vtx[a], Site.Vtx[b]);
						DelaunayEdges.Add(H);

						if (Site.Neighbors[b] == -1)
						{
							Site.bOnHull = true;
							DelaunayHull.Add(Site.Vtx[b]);
						}
					}

					if (Site.Neighbors[a] == -1)
					{
						Site.bOnHull = true;
						DelaunayHull.Add(Site.Vtx[a]);
						//DelaunayHull.Add(Site.Vtx[PCGExMath::Tile(a + 1, 0, 2)]);
					}
				}
			}

			Triangles.Empty();
			Adjacencies.Empty();

			return IsValid;
		}

		void RemoveLongestEdges(const TArrayView<FVector>& Positions)
		{
			uint64 Edge;
			for (const FDelaunaySite2& Site : Sites)
			{
				GetLongestEdge(Positions, Site.Vtx, Edge);
				DelaunayEdges.Remove(Edge);
			}
		}

		void RemoveLongestEdges(const TArrayView<FVector>& Positions, TSet<uint64>& LongestEdges)
		{
			uint64 Edge;
			for (const FDelaunaySite2& Site : Sites)
			{
				GetLongestEdge(Positions, Site.Vtx, Edge);
				DelaunayEdges.Remove(Edge);
				LongestEdges.Add(Edge);
			}
		}

		void GetMergedSites(const int32 SiteIndex, const TSet<uint64>& EdgeConnectors, TSet<int32>& OutMerged, TSet<uint64>& OutUEdges, TBitArray<>& VisitedSites)
		{
			TArray<int32> Stack;

			VisitedSites[SiteIndex] = false;
			Stack.Add(SiteIndex);

			while (!Stack.IsEmpty())
			{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <= 3
				const int32 NextIndex = Stack.Pop(false);
#else
				const int32 NextIndex = Stack.Pop(EAllowShrinking::No);
#endif

				if (VisitedSites[NextIndex]) { continue; }

				OutMerged.Add(NextIndex);
				VisitedSites[NextIndex] = true;

				const FDelaunaySite2* Site = (Sites.GetData() + NextIndex);

				for (int i = 0; i < 3; i++)
				{
					const int32 OtherIndex = Site->Neighbors[i];
					if (OtherIndex == -1 || VisitedSites[OtherIndex]) { continue; }
					const FDelaunaySite2* NeighborSite = Sites.GetData() + OtherIndex;
					if (const uint64 SharedEdge = Site->GetSharedEdge(NeighborSite); EdgeConnectors.Contains(SharedEdge))
					{
						OutUEdges.Add(SharedEdge);
						Stack.Add(OtherIndex);
					}
				}
			}

			VisitedSites[SiteIndex] = true;
		}
	};

	static int32 MTX[4][3] = {
		{0, 1, 2},
		{0, 1, 3},
		{0, 2, 3},
		{1, 2, 3}
	};

	struct FDelaunaySite3
	{
		uint64 Faces[4];
		int32 Vtx[4];
		int32 Neighbors[4];
		int32 Id = -1;
		bool bOnHull = false;

		explicit FDelaunaySite3(const FIntVector4& InVtx, const int32 InId = -1) : Id(InId)
		{
			for (int i = 0; i < 4; i++)
			{
				Vtx[i] = InVtx[i];
				Neighbors[i] = -1;
				Faces[i] = 0;
			}

			std::ranges::sort(Vtx);
		}

		void ComputeFaces()
		{
			for (int i = 0; i < 4; i++) { Faces[i] = PCGEx::H64S(Vtx[MTX[i][0]], Vtx[MTX[i][1]], Vtx[MTX[i][2]]); }
		}

		FORCEINLINE void SetAdjacency(const uint64 Face, const int32 Neighbor)
		{
			for (int i = 0; i < 4; i++)
			{
				if (Faces[i] == Face)
				{
					Neighbors[i] = Neighbor;
					return;
				}
			}
		}
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ TDelaunay3
	{
	public:
		TArray<FDelaunaySite3> Sites;

		TSet<uint64> DelaunayEdges;
		TSet<int32> DelaunayHull;

		bool IsValid = false;

		mutable FRWLock ProcessLock;

		TDelaunay3()
		{
		}

		~TDelaunay3()
		{
			Clear();
		}

		void Clear()
		{
			Sites.Empty();
			DelaunayEdges.Empty();
			DelaunayHull.Empty();

			IsValid = false;
		}

		bool Process(const TArrayView<FVector>& Positions, const bool bComputeFaces = false)
		{
			Clear();
			if (Positions.IsEmpty() || Positions.Num() <= 3) { return false; }

			UE::Geometry::FDelaunay3 Tetrahedralization;

			if (!Tetrahedralization.Triangulate(Positions))
			{
				Clear();
				return false;
			}

			IsValid = true;

			TArray<FIntVector4> Tetrahedra = Tetrahedralization.GetTetrahedra();

			const int32 NumSites = Tetrahedra.Num();
			const int32 NumReserve = NumSites * 3;

			DelaunayEdges.Reserve(NumReserve);

			TMap<uint64, int32> Faces;
			if (bComputeFaces) { Faces.Reserve(NumSites); }

			PCGEx::InitArray(Sites, NumSites);

			for (int i = 0; i < NumSites; i++)
			{
				FDelaunaySite3& Site = Sites[i] = FDelaunaySite3(Tetrahedra[i], i);

				for (int a = 0; a < 4; a++)
				{
					for (int b = a + 1; b < 4; b++)
					{
						DelaunayEdges.Add(PCGEx::H64U(Site.Vtx[a], Site.Vtx[b]));
					}
				}

				if (bComputeFaces)
				{
					Site.ComputeFaces();

					for (int f = 0; f < 4; f++)
					{
						const uint64 FH = Site.Faces[f];
						if (const int32* NeighborId = Faces.Find(FH))
						{
							Faces.Remove(FH);
							Site.SetAdjacency(FH, *NeighborId);
							Sites[*NeighborId].SetAdjacency(FH, i);
						}
						else
						{
							Faces.Add(FH, Site.Id);
						}
					}
				}
			}

			for (FDelaunaySite3& Site : Sites)
			{
				for (int f = 0; f < 4; f++)
				{
					if (Site.Neighbors[f] == -1)
					{
						for (int fi = 0; fi < 3; fi++) { DelaunayHull.Add(Site.Vtx[MTX[f][fi]]); }
						Site.bOnHull = true;
						break;
					}
				}
			}

			Faces.Empty();
			Tetrahedra.Empty();

			return IsValid;
		}

		void RemoveLongestEdges(const TArrayView<FVector>& Positions)
		{
			uint64 Edge;
			for (const FDelaunaySite3& Site : Sites)
			{
				GetLongestEdge(Positions, Site.Vtx, Edge);
				DelaunayEdges.Remove(Edge);
			}
		}

		void RemoveLongestEdges(const TArrayView<FVector>& Positions, TSet<uint64>& LongestEdges)
		{
			uint64 Edge;
			for (const FDelaunaySite3& Site : Sites)
			{
				GetLongestEdge(Positions, Site.Vtx, Edge);
				DelaunayEdges.Remove(Edge);
				LongestEdges.Add(Edge);
			}
		}
	};
}
