// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGeo.h"
#include "PCGExGeoDelaunay.h"

namespace PCGExGeo
{
	class PCGEXTENDEDTOOLKIT_API TVoronoi2
	{
	public:
		TDelaunay2* Delaunay = nullptr;
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
			PCGEX_DELETE(Delaunay)
			Centroids.Empty();
			IsValid = false;
		}

		bool Process(const TArrayView<FVector>& Positions, const FPCGExGeo2DProjectionSettings& ProjectionSettings)
		{
			Clear();

			Delaunay = new TDelaunay2();
			if (!Delaunay->Process(Positions, ProjectionSettings))
			{
				Clear();
				return IsValid;
			}

			const int32 NumSites = Delaunay->Sites.Num();
			Circumcenters.SetNum(NumSites);
			Centroids.SetNum(NumSites);

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
	};

	class PCGEXTENDEDTOOLKIT_API TVoronoi3
	{
	public:
		TDelaunay3* Delaunay = nullptr;
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
			PCGEX_DELETE(Delaunay)
		}

		void Clear()
		{
			PCGEX_DELETE(Delaunay)
			Centroids.Empty();
			IsValid = false;
		}

		bool Process(const TArrayView<FVector>& Positions)
		{
			PCGEX_DELETE(Delaunay)
			IsValid = false;

			Delaunay = new TDelaunay3();
			if (!Delaunay->Process(Positions, true))
			{
				Clear();
				return IsValid;
			}

			const int32 NumSites = Delaunay->Sites.Num();
			Circumspheres.SetNum(NumSites);
			Centroids.SetNum(NumSites);

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
