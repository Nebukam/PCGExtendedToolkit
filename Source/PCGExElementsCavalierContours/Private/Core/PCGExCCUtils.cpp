// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#include "Core/PCGExCCPolyline.h"
#include "Core/PCGExCCMath.h"

namespace PCGExCavalier
{
	// Note: FContourUtils implementations are in PCGExCCPolyline.cpp
	// This file contains additional utility functions

	namespace Utils
	{
		/**
		 * Helper to create a map of root paths from an array of input points
		 */
		TMap<int32, FRootPath> CreateRootPathsMap(const TArray<FInputPoint>& Points, bool bClosed)
		{
			TMap<int32, FRootPath> Result;

			if (Points.IsEmpty())
			{
				return Result;
			}

			// Group points by PathId
			TMap<int32, TArray<const FInputPoint*>> PointsByPath;
			for (const FInputPoint& Pt : Points)
			{
				PointsByPath.FindOrAdd(Pt.PathId).Add(&Pt);
			}

			for (auto& Pair : PointsByPath)
			{
				FRootPath Path(Pair.Key, bClosed);
				Path.Points.Reserve(Pair.Value.Num());

				for (const FInputPoint* Pt : Pair.Value)
				{
					Path.Points.Add(*Pt);
				}

				Result.Add(Pair.Key, MoveTemp(Path));
			}

			return Result;
		}

		/**
		 * Validate that all vertices in a polyline have valid sources
		 */
		bool ValidateVertexSources(const FPolyline& Polyline, TArray<int32>* OutInvalidIndices = nullptr)
		{
			bool bAllValid = true;

			for (int32 i = 0; i < Polyline.VertexCount(); ++i)
			{
				if (!Polyline.GetVertex(i).HasValidSource())
				{
					bAllValid = false;
					if (OutInvalidIndices)
					{
						OutInvalidIndices->Add(i);
					}
				}
			}

			return bAllValid;
		}

		/**
		 * Remap vertex sources to a new path ID
		 */
		void RemapVertexSources(FPolyline& Polyline, int32 OldPathId, int32 NewPathId)
		{
			for (int32 i = 0; i < Polyline.VertexCount(); ++i)
			{
				FVertex& V = Polyline.GetVertex(i);
				if (V.GetPathId() == OldPathId)
				{
					V.Source.PathId = NewPathId;
				}
			}

			// Update path tracking
			if (Polyline.GetPrimaryPathId() == OldPathId)
			{
				Polyline.SetPrimaryPathId(NewPathId);
			}
		}

		/**
		 * Merge sources from multiple paths into a single result
		 */
		TSet<int32> CollectAllPathIds(const TArray<FPolyline>& Polylines)
		{
			TSet<int32> Result;

			for (const FPolyline& Pline : Polylines)
			{
				Result.Append(Pline.GetContributingPathIds());

				for (int32 i = 0; i < Pline.VertexCount(); ++i)
				{
					if (Pline.GetVertex(i).HasValidPath())
					{
						Result.Add(Pline.GetVertex(i).GetPathId());
					}
				}
			}

			return Result;
		}

		/**
		 * Compute statistics about source coverage
		 */
		struct FSourceStats
		{
			int32 TotalVertices = 0;
			int32 ValidSources = 0;
			int32 ValidPaths = 0;
			int32 ValidPoints = 0;
			TMap<int32, int32> VerticesPerPath;

			double GetCoverageRatio() const
			{
				return TotalVertices > 0 ? static_cast<double>(ValidSources) / TotalVertices : 0.0;
			}
		};

		FSourceStats ComputeSourceStats(const FPolyline& Polyline)
		{
			FSourceStats Stats;
			Stats.TotalVertices = Polyline.VertexCount();

			for (int32 i = 0; i < Polyline.VertexCount(); ++i)
			{
				const FVertex& V = Polyline.GetVertex(i);

				if (V.HasValidSource())
				{
					Stats.ValidSources++;
				}

				if (V.HasValidPath())
				{
					Stats.ValidPaths++;
					Stats.VerticesPerPath.FindOrAdd(V.GetPathId())++;
				}

				if (V.Source.HasValidPoint())
				{
					Stats.ValidPoints++;
				}
			}

			return Stats;
		}

		/**
		 * Interpolate source information for vertices without valid sources.
		 * Uses neighboring vertices to estimate missing path IDs.
		 */
		void InterpolateMissingSources(FPolyline& Polyline)
		{
			const int32 N = Polyline.VertexCount();
			if (N < 2)
			{
				return;
			}

			// First pass: identify vertices needing interpolation
			TArray<int32> InvalidIndices;
			for (int32 i = 0; i < N; ++i)
			{
				if (!Polyline.GetVertex(i).HasValidPath())
				{
					InvalidIndices.Add(i);
				}
			}

			if (InvalidIndices.IsEmpty())
			{
				return;
			}

			// Second pass: interpolate from neighbors
			for (int32 InvalidIdx : InvalidIndices)
			{
				// Search for nearest valid neighbors
				int32 PrevValidIdx = INDEX_NONE;
				int32 NextValidIdx = INDEX_NONE;

				// Search backward
				for (int32 Offset = 1; Offset < N; ++Offset)
				{
					const int32 Idx = Polyline.IsClosed()
						                  ? (InvalidIdx - Offset + N) % N
						                  : InvalidIdx - Offset;

					if (Idx < 0)
					{
						break;
					}

					if (Polyline.GetVertex(Idx).HasValidPath())
					{
						PrevValidIdx = Idx;
						break;
					}
				}

				// Search forward
				for (int32 Offset = 1; Offset < N; ++Offset)
				{
					const int32 Idx = Polyline.IsClosed()
						                  ? (InvalidIdx + Offset) % N
						                  : InvalidIdx + Offset;

					if (Idx >= N)
					{
						break;
					}

					if (Polyline.GetVertex(Idx).HasValidPath())
					{
						NextValidIdx = Idx;
						break;
					}
				}

				// Apply interpolation
				FVertex& V = Polyline.GetVertex(InvalidIdx);

				if (PrevValidIdx != INDEX_NONE && NextValidIdx != INDEX_NONE)
				{
					// Use the more common path between neighbors
					const int32 PrevPath = Polyline.GetVertex(PrevValidIdx).GetPathId();
					const int32 NextPath = Polyline.GetVertex(NextValidIdx).GetPathId();

					// Choose based on distance
					const double DistToPrev = FVector2D::Distance(
						V.GetPosition(), Polyline.GetVertex(PrevValidIdx).GetPosition());
					const double DistToNext = FVector2D::Distance(
						V.GetPosition(), Polyline.GetVertex(NextValidIdx).GetPosition());

					V.Source = FVertexSource::FromPath(DistToPrev <= DistToNext ? PrevPath : NextPath);
				}
				else if (PrevValidIdx != INDEX_NONE)
				{
					V.Source = FVertexSource::FromPath(Polyline.GetVertex(PrevValidIdx).GetPathId());
				}
				else if (NextValidIdx != INDEX_NONE)
				{
					V.Source = FVertexSource::FromPath(Polyline.GetVertex(NextValidIdx).GetPathId());
				}
			}
		}
	}
}
