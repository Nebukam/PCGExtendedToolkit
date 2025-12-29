// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#pragma once

#include "Core/PCGExCCPolyline.h"
#include "Core/PCGExCCMath.h"

namespace PCGExCavalier
{
	// Note: FContourUtils implementations are in PCGExCCPolyline.cpp
	// This file contains additional utility functions

	namespace Utils
	{		
		/**
		 * Validate that all vertices in a polyline have valid sources
		 */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		bool ValidateVertexSources(const FPolyline& Polyline, TArray<int32>* OutInvalidIndices = nullptr);

		/**
		 * Remap vertex sources to a new path ID
		 */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		void RemapVertexSources(FPolyline& Polyline, int32 OldPathId, int32 NewPathId);

		/**
		 * Merge sources from multiple paths into a single result
		 */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		TSet<int32> CollectAllPathIds(const TArray<FPolyline>& Polylines);

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

		PCGEXELEMENTSCAVALIERCONTOURS_API
		FSourceStats ComputeSourceStats(const FPolyline& Polyline);

		/**
		 * Interpolate source information for vertices without valid sources.
		 * Uses neighboring vertices to estimate missing path IDs.
		 */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		void InterpolateMissingSources(FPolyline& Polyline);
		
		PCGEXELEMENTSCAVALIERCONTOURS_API
		void AddFuzzinessToPositions(FPolyline& Polyline);
		
	}
}
