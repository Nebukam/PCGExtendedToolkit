// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGeo.h"

struct FPCGExGeo2DProjectionDetails;

namespace PCGExMath::Geo
{
	class TDelaunay3;
	class TDelaunay2;

	class PCGEXCORE_API TVoronoi2
	{
	public:
		TSharedPtr<TDelaunay2> Delaunay;
		TSet<uint64> VoronoiEdges;
		TArray<FVector> Circumcenters;
		TArray<FVector> Centroids;

		// Metric used for this Voronoi diagram
		EPCGExVoronoiMetric Metric = EPCGExVoronoiMetric::Euclidean;

		// Extended output for L1/L∞ metrics
		// For Euclidean, these mirror Circumcenters/Centroids and VoronoiEdges
		// For L1/L∞, OutputVertices contains [CellCenters..., BendPoints...] and OutputEdges contains subdivided edges
		TArray<FVector> OutputVertices;
		TArray<uint64> OutputEdges;
		int32 NumCellCenters = 0; // Number of cell centers (first N entries in OutputVertices)

		bool IsValid = false;

		TVoronoi2() = default;
		~TVoronoi2();

	protected:
		void Clear();

		// Build extended output with projection support (projects to 2D, computes circumcenters, unprojects back)
		void BuildMetricOutput(const TArrayView<FVector>& Positions, const FPCGExGeo2DProjectionDetails& ProjectionDetails, EPCGExCellCenter CellCenterMethod, const FBox* Bounds = nullptr, TBitArray<>* WithinBounds = nullptr);

	public:
		bool Process(const TArrayView<FVector>& Positions, const FPCGExGeo2DProjectionDetails& ProjectionDetails);
		bool Process(const TArrayView<FVector>& Positions, const FPCGExGeo2DProjectionDetails& ProjectionDetails, const FBox& Bounds, TBitArray<>& WithinBounds);

		// Process with metric support
		bool Process(const TArrayView<FVector>& Positions, const FPCGExGeo2DProjectionDetails& ProjectionDetails, EPCGExVoronoiMetric InMetric, EPCGExCellCenter CellCenterMethod);
		bool Process(const TArrayView<FVector>& Positions, const FPCGExGeo2DProjectionDetails& ProjectionDetails, const FBox& Bounds, TBitArray<>& WithinBounds, EPCGExVoronoiMetric InMetric, EPCGExCellCenter CellCenterMethod);
	};

	class PCGEXCORE_API TVoronoi3
	{
	public:
		TSharedPtr<TDelaunay3> Delaunay;
		TSet<uint64> VoronoiEdges;
		TSet<int32> VoronoiHull;
		TArray<FSphere> Circumspheres;
		TArray<FVector> Centroids;

		bool IsValid = false;

		TVoronoi3() = default;
		~TVoronoi3();

	protected:
		void Clear();

	public:
		bool Process(const TArrayView<FVector>& Positions);
	};
}
