// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

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

		bool IsValid = false;

		TVoronoi2() = default;
		~TVoronoi2();

	protected:
		void Clear();

	public:
		bool Process(const TArrayView<FVector>& Positions, const FPCGExGeo2DProjectionDetails& ProjectionDetails);
		bool Process(const TArrayView<FVector>& Positions, const FPCGExGeo2DProjectionDetails& ProjectionDetails, const FBox& Bounds, TBitArray<>& WithinBounds);
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
