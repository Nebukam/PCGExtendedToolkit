// Copyright 2025 Timothé Lapetite and contributors
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

		bool Process(const TArrayView<FVector>& Positions, const FPCGExGeo2DProjectionDetails& ProjectionDetails, const FBox& Bounds, TBitArray<>& WithinBounds)
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

			for (FDelaunaySite2& Site : Delaunay->Sites)
			{
				FVector CC = FVector::ZeroVector;

				GetCircumcenter(Positions, Site.Vtx, CC);
				Circumcenters[Site.Id] = CC;

				WithinBounds[Site.Id] = Bounds.IsInside(CC);

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

			if (!Delaunay->Process<true, false>(Positions))
			{
				Clear();
				return IsValid;
			}

			const int32 NumSites = Delaunay->Sites.Num();
			PCGEx::InitArray(Circumspheres, NumSites);
			PCGEx::InitArray(Centroids, NumSites);

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(GeoVoronoi::FindVoronoiEdges);

				for (FDelaunaySite3& Site : Delaunay->Sites)
				{
					FindSphereFrom4Points(Positions, Site.Vtx, Circumspheres[Site.Id]);
					GetCentroid(Positions, Site.Vtx, Centroids[Site.Id]);
				}

				for (const TPair<uint32, uint64>& AdjacencyPair : Delaunay->Adjacency)
				{
					int32 A = -1;
					int32 B = -1;
					PCGEx::NH64(AdjacencyPair.Value, A, B);

					if (A == -1 || B == -1) { continue; }

					VoronoiEdges.Add(PCGEx::H64U(A, B));
				}
			}

			IsValid = true;
			return IsValid;
		}
	};
}
