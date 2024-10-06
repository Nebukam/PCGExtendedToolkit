// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGeo.h"
#include "PCGExGeoDelaunay.h"

namespace PCGExGeo
{
	class /*PCGEXTENDEDTOOLKIT_API*/ TVoronoi2
	{
	public:
		TUniquePtr<TDelaunay2> Delaunay;
		TSet<uint64> VoronoiEdges;
		TArray<FVector> Circumcenters;
		TArray<FVector> Centroids;

		bool IsValid = false;

		TVoronoi2()
		{
		}

		~TVoronoi2()
		{
			Clear();
		}

		void Clear()
		{
			Delaunay.Reset();
			Centroids.Empty();
			IsValid = false;
		}

		bool Process(const TArrayView<FVector>& Positions, const FPCGExGeo2DProjectionDetails& ProjectionDetails)
		{
			Clear();

			Delaunay = MakeUnique<TDelaunay2>();
			if (!Delaunay->Process(Positions, ProjectionDetails))
			{
				Clear();
				return IsValid;
			}

			const int32 NumSites = Delaunay->Sites.Num();
			PCGEx::InitArray(Circumcenters, NumSites);
			PCGEx::InitArray(Centroids, NumSites);

			for (FDelaunaySite2& Site : Delaunay->Sites)
			{
				GetCircumcenter(Positions, Site.Vtx, Circumcenters[Site.Id]);
				GetCentroid(Positions, Site.Vtx, Centroids[Site.Id]);

				for (int i = 0; i < 3; i++)
				{
					const int32 AdjacentIdx = Site.Neighbors[i];

					if (AdjacentIdx == -1) { continue; }

					VoronoiEdges.Add(PCGEx::H64U(Site.Id, AdjacentIdx));
				}
			}

			IsValid = true;
			return IsValid;
		}

		template <bool bPerVtxCheck>
		bool Process(const TArrayView<FVector>& Positions, const FPCGExGeo2DProjectionDetails& ProjectionDetails, const FBox& Bounds, TBitArray<>& WithinBounds, TBitArray<>& VtxWithinBounds)
		{
			Clear();

			Delaunay = MakeUnique<TDelaunay2>();
			if (!Delaunay->Process(Positions, ProjectionDetails))
			{
				Clear();
				return IsValid;
			}

			const int32 NumSites = Delaunay->Sites.Num();
			PCGEx::InitArray(Circumcenters, NumSites);
			PCGEx::InitArray(Centroids, NumSites);
			WithinBounds.Init(true, NumSites);

			if constexpr (bPerVtxCheck)
			{
				VtxWithinBounds.Init(true, Positions.Num());
			}

			for (FDelaunaySite2& Site : Delaunay->Sites)
			{
				FVector CC = FVector::ZeroVector;

				GetCircumcenter(Positions, Site.Vtx, CC);
				Circumcenters[Site.Id] = CC;

				if constexpr (bPerVtxCheck)
				{
					if (Bounds.IsInside(CC))
					{
						WithinBounds[Site.Id] = true;
					}
					else
					{
						WithinBounds[Site.Id] = false;
						for (int i = 0; i < 3; i++) { VtxWithinBounds[Site.Vtx[i]] = false; }
					}
				}
				else
				{
					WithinBounds[Site.Id] = Bounds.IsInside(CC);
				}

				GetCentroid(Positions, Site.Vtx, Centroids[Site.Id]);

				for (int i = 0; i < 3; i++)
				{
					const int32 AdjacentIdx = Site.Neighbors[i];

					if (AdjacentIdx == -1) { continue; }

					VoronoiEdges.Add(PCGEx::H64U(Site.Id, AdjacentIdx));
				}
			}

			IsValid = true;
			return IsValid;
		}
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ TVoronoi3
	{
	public:
		TUniquePtr<TDelaunay3> Delaunay;
		TSet<uint64> VoronoiEdges;
		TSet<int32> VoronoiHull;
		TArray<FSphere> Circumspheres;
		TArray<FVector> Centroids;

		bool IsValid = false;

		TVoronoi3()
		{
		}

		~TVoronoi3()
		{
		}

		void Clear()
		{
			Delaunay.Reset();
			Centroids.Empty();
			IsValid = false;
		}

		bool Process(const TArrayView<FVector>& Positions)
		{
			IsValid = false;
			Delaunay = MakeUnique<TDelaunay3>();

			if (!Delaunay->Process(Positions, true))
			{
				Clear();
				return IsValid;
			}

			const int32 NumSites = Delaunay->Sites.Num();
			PCGEx::InitArray(Circumspheres, NumSites);
			PCGEx::InitArray(Centroids, NumSites);

			for (FDelaunaySite3& Site : Delaunay->Sites)
			{
				FindSphereFrom4Points(Positions, Site.Vtx, Circumspheres[Site.Id]);
				GetCentroid(Positions, Site.Vtx, Centroids[Site.Id]);

				for (int i = 0; i < 4; i++)
				{
					const int32 AdjacentIdx = Site.Neighbors[i];

					if (AdjacentIdx == -1)
					{
						//VoronoiHull.Add(); //TODO: Find which triangle has no adjacency
						Site.bOnHull = true;
						continue;
					}

					VoronoiEdges.Add(PCGEx::H64U(Site.Id, AdjacentIdx));
				}
			}

			IsValid = true;
			return IsValid;
		}
	};
}
