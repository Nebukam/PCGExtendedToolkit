// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#include "Core/PCGExCCUtils.h"
#include "Core/PCGExCCPolyline.h"
#include "Core/PCGExCCMath.h"
#include "Math/RandomStream.h"

namespace PCGExCavalier
{
	// Note: FContourUtils implementations are in PCGExCCPolyline.cpp
	// This file contains additional utility functions

	namespace Utils
	{
		/**
		 * Validate that all vertices in a polyline have valid sources
		 */
		bool ValidateVertexSources(const FPolyline& Polyline, TArray<int32>* OutInvalidIndices)
		{
			bool bAllValid = true;

			for (int32 i = 0; i < Polyline.VertexCount(); ++i)
			{
				if (!Polyline.GetVertex(i).HasValidSource())
				{
					bAllValid = false;
					if (OutInvalidIndices) { OutInvalidIndices->Add(i); }
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
				if (!Polyline.GetVertex(i).HasValidPath()) { InvalidIndices.Add(i); }
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
					const int32 Idx =
						Polyline.IsClosed()
							? (InvalidIdx - Offset + N) % N
							: InvalidIdx - Offset;

					if (Idx < 0) { break; }

					if (Polyline.GetVertex(Idx).HasValidPath())
					{
						PrevValidIdx = Idx;
						break;
					}
				}

				// Search forward
				for (int32 Offset = 1; Offset < N; ++Offset)
				{
					const int32 Idx =
						Polyline.IsClosed()
							? (InvalidIdx + Offset) % N
							: InvalidIdx + Offset;

					if (Idx >= N) { break; }

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
					const double DistToPrev = FVector2D::Distance(V.GetPosition(), Polyline.GetVertex(PrevValidIdx).GetPosition());
					const double DistToNext = FVector2D::Distance(V.GetPosition(), Polyline.GetVertex(NextValidIdx).GetPosition());

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

		void AddFuzzinessToPositions(FPolyline& Polyline)
		{
			const int32 N = Polyline.VertexCount();
			FRandomStream RandomStream(N);
			for (FVertex& V : Polyline.GetVertices())
			{
				V.Position.X += RandomStream.FRandRange(-0.001, 0.001);
				V.Position.Y += RandomStream.FRandRange(-0.001, 0.001);
			}
		}
	}
}
