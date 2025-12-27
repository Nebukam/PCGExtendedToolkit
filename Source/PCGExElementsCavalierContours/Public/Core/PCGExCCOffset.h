// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#pragma once

#include "CoreMinimal.h"
#include "PCGExCCTypes.h"
#include "PCGExCCPolyline.h"
#include "Details/PCGExCCDetails.h"

namespace PCGExCavalier
{
	namespace Offset
	{
		/**
		 * Internal structures for self-intersection handling
		 */
		namespace Internal
		{
			/** Basic intersection point during offset */
			struct FBasicIntersect
			{
				int32 StartIndex1;
				int32 StartIndex2;
				FVector2D Point;

				FBasicIntersect()
					: StartIndex1(0)
					, StartIndex2(0)
					, Point(FVector2D::ZeroVector)
				{
				}

				FBasicIntersect(int32 InIdx1, int32 InIdx2, const FVector2D& InPoint)
					: StartIndex1(InIdx1)
					, StartIndex2(InIdx2)
					, Point(InPoint)
				{
				}
			};

			/** Slice of polyline between self-intersection points */
			struct FPolylineSlice
			{
				int32 StartIndex = 0;
				int32 EndIndexOffset = 0;
				FVertex UpdatedStart;
				double UpdatedEndBulge = 0.0;
				FVector2D EndPoint = FVector2D::ZeroVector;
				FVertexSource EndSource;  // Source info for the end point

				FVector2D GetStartPoint() const { return UpdatedStart.GetPosition(); }
				int32 VertexCount() const { return EndIndexOffset + 2; }
			};

			/** Raw offset segment before joining */
			struct FRawOffsetSeg
			{
				FVertex V1;
				FVertex V2;
				FVector2D OrigV1Pos;
				FVector2D OrigV2Pos;
				bool bCollapsedArc = false;
			};
		}


		/**
		 * Compute parallel offset of a polyline.
		 * 
		 * Positive offset values offset outward (for CCW orientation) or inward (for CW).
		 * Negative offset values offset in the opposite direction.
		 * 
		 * Source tracking: Each vertex in the result inherits its Source from the
		 * corresponding vertex in the input polyline.
		 * 
		 * @param Polyline The input polyline to offset
		 * @param Offset The offset distance (positive = outward for CCW)
		 * @param Options Offset options
		 * @return Array of offset polylines (may be multiple due to self-intersection handling)
		 */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		TArray<FPolyline> ParallelOffset(
			const FPolyline& Polyline,
			double Offset,
			const FPCGExCCOffsetOptions& Options = FPCGExCCOffsetOptions());


		/**
		 * Compute parallel offset without self-intersection handling.
		 * Faster but may produce self-intersecting results.
		 * 
		 * @param Polyline The input polyline to offset
		 * @param Offset The offset distance
		 * @param PosEqualEps Position equality epsilon
		 * @return Single offset polyline (may self-intersect)
		 */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		FPolyline RawParallelOffset(
			const FPolyline& Polyline,
			double Offset,
			double PosEqualEps = 1e-5);


		/**
		 * Internal functions exposed for testing and advanced usage
		 */
		namespace Internal
		{
			/** Create raw offset segments from polyline */
			PCGEXELEMENTSCAVALIERCONTOURS_API
			TArray<FRawOffsetSeg> CreateRawOffsetSegments(
				const FPolyline& Polyline,
				double Offset);

			/** Create raw offset polyline from segments */
			PCGEXELEMENTSCAVALIERCONTOURS_API
			FPolyline CreateRawOffsetPolyline(
				const FPolyline& OriginalPolyline,
				const TArray<FRawOffsetSeg>& Segments,
				double Offset,
				double PosEqualEps);

			/** Find all self-intersections in a polyline */
			PCGEXELEMENTSCAVALIERCONTOURS_API
			TArray<FBasicIntersect> FindAllSelfIntersections(
				const FPolyline& Polyline,
				const FPolyline::FApproxAABBIndex& Index,
				double PosEqualEps);

			/** Create slices using dual offset method */
			PCGEXELEMENTSCAVALIERCONTOURS_API
			TArray<FPolylineSlice> CreateSlices(
				const FPolyline& Original,
				const FPolyline& RawOffset,
				const FPolyline& DualRawOffset,
				const FPolyline::FApproxAABBIndex& OrigIndex,
				double Offset,
				double PosEqualEps,
				double OffsetTolerance);

			/** Find intersections between two polylines */
			PCGEXELEMENTSCAVALIERCONTOURS_API
			TArray<FBasicIntersect> FindIntersectsBetween(
				const FPolyline& Pline1,
				const FPolyline& Pline2,
				const FPolyline::FApproxAABBIndex& Index1,
				double PosEqualEps);

			/** Validate if a point is valid for the given offset distance */
			PCGEXELEMENTSCAVALIERCONTOURS_API
			bool PointValidForOffset(
				const FPolyline& OriginalPolyline,
				const FPolyline::FApproxAABBIndex& OrigIndex,
				double Offset,
				const FVector2D& Point,
				double PosEqualEps,
				double OffsetTolerance);

			/** Stitch slices together into closed polylines */
			PCGEXELEMENTSCAVALIERCONTOURS_API
			TArray<FPolyline> StitchSlices(
				const FPolyline& RawOffsetPolyline,
				const TArray<FPolylineSlice>& Slices,
				bool bOriginalIsClosed,
				int32 SourcePathId,
				double JoinEps,
				double PosEqualEps);
		}
	}
}