// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Math/Geo/PCGExVoronoi.h"

#include "CoreMinimal.h"
#include "Helpers/PCGExArrayHelpers.h"

#include "Math/Geo/PCGExDelaunay.h"
#include "Math/Geo/PCGExGeo.h"
#include "Math/PCGExProjectionDetails.h"

namespace PCGExMath::Geo
{
	TVoronoi2::~TVoronoi2()
	{
		Clear();
	}

	void TVoronoi2::Clear()
	{
		Delaunay.Reset();
		Centroids.Empty();
		Circumcenters.Empty();
		VoronoiEdges.Empty();
		OutputVertices.Empty();
		OutputEdges.Empty();
		NumCellCenters = 0;
		Metric = EPCGExVoronoiMetric::Euclidean;
		IsValid = false;
	}

	bool TVoronoi2::Process(const TArrayView<FVector>& Positions, const FPCGExGeo2DProjectionDetails& ProjectionDetails)
	{
		Clear();

		Delaunay = MakeShared<TDelaunay2>();
		if (!Delaunay->Process(Positions, ProjectionDetails))
		{
			Clear();
			return IsValid;
		}

		const int32 NumSites = Delaunay->Sites.Num();
		PCGExArrayHelpers::InitArray(Circumcenters, NumSites);
		PCGExArrayHelpers::InitArray(Centroids, NumSites);

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

	bool TVoronoi2::Process(const TArrayView<FVector>& Positions, const FPCGExGeo2DProjectionDetails& ProjectionDetails, const FBox& Bounds, TBitArray<>& WithinBounds)
	{
		Clear();

		Delaunay = MakeShared<TDelaunay2>();
		if (!Delaunay->Process(Positions, ProjectionDetails))
		{
			Clear();
			return IsValid;
		}

		const int32 NumSites = Delaunay->Sites.Num();
		PCGExArrayHelpers::InitArray(Circumcenters, NumSites);
		PCGExArrayHelpers::InitArray(Centroids, NumSites);
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

	bool TVoronoi2::Process(const TArrayView<FVector>& Positions, const FPCGExGeo2DProjectionDetails& ProjectionDetails, EPCGExVoronoiMetric InMetric, EPCGExCellCenter CellCenterMethod)
	{
		Clear();
		Metric = InMetric;

		Delaunay = MakeShared<TDelaunay2>();
		if (!Delaunay->Process(Positions, ProjectionDetails))
		{
			Clear();
			return IsValid;
		}

		const int32 NumSites = Delaunay->Sites.Num();
		PCGExArrayHelpers::InitArray(Circumcenters, NumSites);
		PCGExArrayHelpers::InitArray(Centroids, NumSites);

		for (FDelaunaySite2& Site : Delaunay->Sites)
		{
			// Store 3D versions for backwards compatibility
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
		BuildMetricOutput(Positions, ProjectionDetails, CellCenterMethod, nullptr, nullptr);
		return IsValid;
	}

	bool TVoronoi2::Process(const TArrayView<FVector>& Positions, const FPCGExGeo2DProjectionDetails& ProjectionDetails, const FBox& Bounds, TBitArray<>& WithinBounds, EPCGExVoronoiMetric InMetric, EPCGExCellCenter CellCenterMethod)
	{
		Clear();
		Metric = InMetric;

		Delaunay = MakeShared<TDelaunay2>();
		if (!Delaunay->Process(Positions, ProjectionDetails))
		{
			Clear();
			return IsValid;
		}

		const int32 NumSites = Delaunay->Sites.Num();
		PCGExArrayHelpers::InitArray(Circumcenters, NumSites);
		PCGExArrayHelpers::InitArray(Centroids, NumSites);
		WithinBounds.Init(true, NumSites);

		for (FDelaunaySite2& Site : Delaunay->Sites)
		{
			// Store 3D versions for backwards compatibility
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
		// BuildMetricOutput will compute final positions and check bounds after unprojection
		BuildMetricOutput(Positions, ProjectionDetails, CellCenterMethod, &Bounds, &WithinBounds);
		return IsValid;
	}

	void TVoronoi2::BuildMetricOutput(const TArrayView<FVector>& Positions, const FPCGExGeo2DProjectionDetails& ProjectionDetails, EPCGExCellCenter CellCenterMethod, const FBox* Bounds, TBitArray<>* WithinBounds)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TVoronoi2::BuildMetricOutput);

		const int32 NumSites = Delaunay->Sites.Num();
		const int32 NumPositions = Positions.Num();
		NumCellCenters = NumSites;

		// Reserve space for cell centers + potential bend points
		OutputVertices.Reserve(NumSites + VoronoiEdges.Num());
		OutputEdges.Reserve(VoronoiEdges.Num() * 2);

		// Build cell centers
		OutputVertices.SetNum(NumSites);

		// Initialize WithinBounds if provided
		if (WithinBounds)
		{
			WithinBounds->Init(true, NumSites);
		}

		// Project all positions to 2D space - used for all metrics
		TArray<FVector> ProjectedPositions;
		ProjectedPositions.SetNumUninitialized(NumPositions);
		for (int32 i = 0; i < NumPositions; i++)
		{
			ProjectedPositions[i] = ProjectionDetails.Project(Positions[i]);
		}
		const TArrayView<FVector> ProjectedView(ProjectedPositions);

		// Compute cell centers in projected space, then unproject
		for (int32 i = 0; i < NumSites; i++)
		{
			const FDelaunaySite2& Site = Delaunay->Sites[i];

			FVector ProjectedCenter;
			if (CellCenterMethod == EPCGExCellCenter::Centroid)
			{
				GetCentroid(ProjectedView, Site.Vtx, ProjectedCenter);
			}
			else if (CellCenterMethod == EPCGExCellCenter::Circumcenter)
			{
				GetCircumcenter2D(ProjectedView, Site.Vtx, ProjectedCenter);
			}
			else // Balanced
			{
				GetCircumcenter2D(ProjectedView, Site.Vtx, ProjectedCenter);
				if (Bounds)
				{
					const FVector Unprojected = ProjectionDetails.Unproject(ProjectedCenter);
					if (!Bounds->IsInside(Unprojected))
					{
						GetCentroid(ProjectedView, Site.Vtx, ProjectedCenter);
					}
				}
			}

			OutputVertices[i] = ProjectionDetails.Unproject(ProjectedCenter);

			if (Bounds && WithinBounds)
			{
				(*WithinBounds)[i] = Bounds->IsInside(OutputVertices[i]);
			}
		}

		// Process edges - for Euclidean, just direct edges; for L1/L∞, potential bends
		for (const uint64 EdgeHash : VoronoiEdges)
		{
			const int32 SiteA = PCGEx::H64A(EdgeHash);
			const int32 SiteB = PCGEx::H64B(EdgeHash);

			// For Euclidean, just create direct edges (no bends)
			if (Metric == EPCGExVoronoiMetric::Euclidean)
			{
				OutputEdges.Add(PCGEx::H64(SiteA, SiteB));
				continue;
			}

			// For L1/L∞, compute 2D path with potential bends
			const FDelaunaySite2& SiteDataA = Delaunay->Sites[SiteA];
			const FDelaunaySite2& SiteDataB = Delaunay->Sites[SiteB];

			// Get projected cell centers for 2D path computation
			FVector ProjectedCenterA, ProjectedCenterB;
			if (CellCenterMethod == EPCGExCellCenter::Centroid)
			{
				GetCentroid(ProjectedView, SiteDataA.Vtx, ProjectedCenterA);
				GetCentroid(ProjectedView, SiteDataB.Vtx, ProjectedCenterB);
			}
			else if (CellCenterMethod == EPCGExCellCenter::Circumcenter)
			{
				GetCircumcenter2D(ProjectedView, SiteDataA.Vtx, ProjectedCenterA);
				GetCircumcenter2D(ProjectedView, SiteDataB.Vtx, ProjectedCenterB);
			}
			else // Balanced
			{
				const bool bInBoundsA = WithinBounds ? (*WithinBounds)[SiteA] : true;
				const bool bInBoundsB = WithinBounds ? (*WithinBounds)[SiteB] : true;
				if (bInBoundsA) { GetCircumcenter2D(ProjectedView, SiteDataA.Vtx, ProjectedCenterA); }
				else { GetCentroid(ProjectedView, SiteDataA.Vtx, ProjectedCenterA); }
				if (bInBoundsB) { GetCircumcenter2D(ProjectedView, SiteDataB.Vtx, ProjectedCenterB); }
				else { GetCentroid(ProjectedView, SiteDataB.Vtx, ProjectedCenterB); }
			}

			// Use projected X,Y for 2D path computation
			const FVector2D Start2D(ProjectedCenterA.X, ProjectedCenterA.Y);
			const FVector2D End2D(ProjectedCenterB.X, ProjectedCenterB.Y);

			TArray<FVector2D> Path2D;
			if (Metric == EPCGExVoronoiMetric::Manhattan)
			{
				ComputeL1EdgePath(Start2D, End2D, Path2D);
			}
			else // Chebyshev
			{
				ComputeLInfEdgePath(Start2D, End2D, Path2D);
			}

			if (Path2D.Num() == 2)
			{
				// No bend point, direct edge
				OutputEdges.Add(PCGEx::H64(SiteA, SiteB));
			}
			else
			{
				// Has bend points - add intermediate vertices
				int32 PrevIdx = SiteA;

				for (int32 j = 1; j < Path2D.Num() - 1; j++)
				{
					// Interpolate Z in projected space based on position along path
					const double Alpha = static_cast<double>(j) / (Path2D.Num() - 1);
					const double ProjectedZ = FMath::Lerp(ProjectedCenterA.Z, ProjectedCenterB.Z, Alpha);

					// Create projected bend point and unproject to get point on the plane
					const FVector ProjectedBend(Path2D[j].X, Path2D[j].Y, ProjectedZ);
					const FVector BendPoint3D = ProjectionDetails.Unproject(ProjectedBend);

					const int32 BendIdx = OutputVertices.Num();
					OutputVertices.Add(BendPoint3D);

					// For non-Euclidean metrics, if bend point is out of bounds, mark connected sites as out of bounds
					if (Bounds && WithinBounds && !Bounds->IsInside(BendPoint3D))
					{
						(*WithinBounds)[SiteA] = false;
						(*WithinBounds)[SiteB] = false;
					}

					// Edge from previous to bend
					OutputEdges.Add(PCGEx::H64(PrevIdx, BendIdx));
					PrevIdx = BendIdx;
				}

				// Final edge from last bend to end
				OutputEdges.Add(PCGEx::H64(PrevIdx, SiteB));
			}
		}
	}

	TVoronoi3::~TVoronoi3()
	{
		Clear();
	}

	void TVoronoi3::Clear()
	{
		Delaunay.Reset();
		Centroids.Empty();
		IsValid = false;
	}

	bool TVoronoi3::Process(const TArrayView<FVector>& Positions)
	{
		IsValid = false;
		Delaunay = MakeShared<TDelaunay3>();

		if (!Delaunay->Process<true, false>(Positions))
		{
			Clear();
			return IsValid;
		}

		const int32 NumSites = Delaunay->Sites.Num();
		PCGExArrayHelpers::InitArray(Circumspheres, NumSites);
		PCGExArrayHelpers::InitArray(Centroids, NumSites);

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
}
