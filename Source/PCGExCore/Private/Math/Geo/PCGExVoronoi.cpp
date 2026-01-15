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
		Metric = InMetric;
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
		BuildMetricOutput(Positions, CellCenterMethod, nullptr);
		return IsValid;
	}

	bool TVoronoi2::Process(const TArrayView<FVector>& Positions, const FPCGExGeo2DProjectionDetails& ProjectionDetails, const FBox& Bounds, TBitArray<>& WithinBounds, EPCGExVoronoiMetric InMetric, EPCGExCellCenter CellCenterMethod)
	{
		Metric = InMetric;
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
			// Use 2D circumcenter for bounds checking (consistent X,Y regardless of Z)
			FVector CC2D = FVector::ZeroVector;
			GetCircumcenter2D(Positions, Site.Vtx, CC2D);
			WithinBounds[Site.Id] = Bounds.IsInside(CC2D);

			// Also store 3D versions for backwards compatibility
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
		BuildMetricOutput(Positions, CellCenterMethod, &WithinBounds);
		return IsValid;
	}

	void TVoronoi2::BuildMetricOutput(const TArrayView<FVector>& Positions, EPCGExCellCenter CellCenterMethod, const TBitArray<>* WithinBounds)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TVoronoi2::BuildMetricOutput);

		const int32 NumSites = Delaunay->Sites.Num();
		NumCellCenters = NumSites;

		// Reserve space for cell centers + potential bend points
		OutputVertices.Reserve(NumSites + VoronoiEdges.Num());
		OutputEdges.Reserve(VoronoiEdges.Num() * 2);

		// Build cell centers using 2D circumcenters (X,Y from 2D computation, Z averaged from vertices)
		// This ensures the cell centers have correct X,Y positions regardless of input Z values
		OutputVertices.SetNum(NumSites);

		for (int32 i = 0; i < NumSites; i++)
		{
			const FDelaunaySite2& Site = Delaunay->Sites[i];

			if (CellCenterMethod == EPCGExCellCenter::Centroid)
			{
				// Centroid: average of all three vertices (same result in 2D and 3D for X,Y)
				GetCentroid(Positions, Site.Vtx, OutputVertices[i]);
			}
			else if (CellCenterMethod == EPCGExCellCenter::Circumcenter)
			{
				// Use 2D circumcenter for correct X,Y, with averaged Z
				GetCircumcenter2D(Positions, Site.Vtx, OutputVertices[i]);
			}
			else // Balanced
			{
				const bool bInBounds = WithinBounds ? (*WithinBounds)[i] : true;
				if (bInBounds)
				{
					GetCircumcenter2D(Positions, Site.Vtx, OutputVertices[i]);
				}
				else
				{
					GetCentroid(Positions, Site.Vtx, OutputVertices[i]);
				}
			}
		}

		// Process each Voronoi edge
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
			const FVector& CenterA = OutputVertices[SiteA];
			const FVector& CenterB = OutputVertices[SiteB];

			// Use X,Y directly for 2D path computation (circumcenters already have correct 2D X,Y)
			const FVector2D Start2D(CenterA.X, CenterA.Y);
			const FVector2D End2D(CenterB.X, CenterB.Y);

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
				// Has bend points - add intermediate vertices with interpolated Z
				int32 PrevIdx = SiteA;

				for (int32 i = 1; i < Path2D.Num() - 1; i++)
				{
					// Interpolate Z based on position along path
					const double Alpha = static_cast<double>(i) / (Path2D.Num() - 1);
					const double Z = FMath::Lerp(CenterA.Z, CenterB.Z, Alpha);

					// Create 3D bend point
					const FVector BendPoint3D(Path2D[i].X, Path2D[i].Y, Z);

					const int32 BendIdx = OutputVertices.Num();
					OutputVertices.Add(BendPoint3D);

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
