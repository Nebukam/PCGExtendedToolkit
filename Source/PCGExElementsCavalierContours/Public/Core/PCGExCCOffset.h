// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#pragma once

#include "CoreMinimal.h"
#include "PCGExCCCommon.h"
#include "PCGExCCTypes.h"
#include "PCGExCCPolyline.h"
#include "PCGExCCMath.h"
#include "Details/PCGExCCDetails.h"

namespace PCGExCavalier::Offset
{
	/**
	 * Pre-computed segment data to avoid redundant calculations.
	 * Caches arc geometry and bounding boxes for each segment.
	 */
	struct PCGEXELEMENTSCAVALIERCONTOURS_API FCachedSegment
	{
		/** Arc geometry (valid only if bIsArc is true) */
		Math::FArcGeometry Arc;
		
		/** Axis-aligned bounding box for the segment */
		double MinX, MinY, MaxX, MaxY;
		
		/** Whether this segment is an arc (vs line) */
		bool bIsArc = false;
		
		/** Whether the arc geometry is valid */
		bool bArcValid = false;

		FCachedSegment() = default;
		
		FORCEINLINE bool Overlaps(double QMinX, double QMinY, double QMaxX, double QMaxY) const
		{
			return !(MaxX < QMinX || MinX > QMaxX || MaxY < QMinY || MinY > QMaxY);
		}
	};

	/**
	 * Grid-based spatial index for O(1) average-case segment queries.
	 * Significantly faster than linear AABB scanning for large polylines.
	 */
	struct PCGEXELEMENTSCAVALIERCONTOURS_API FGridSpatialIndex
	{
	private:
		/** Grid cells containing segment indices */
		TArray<TArray<int32>> Cells;
		
		/** Grid dimensions */
		int32 GridSizeX = 0;
		int32 GridSizeY = 0;
		
		/** World bounds */
		double WorldMinX = 0, WorldMinY = 0;
		double WorldMaxX = 0, WorldMaxY = 0;
		
		/** Cell size */
		double CellWidth = 1.0;
		double CellHeight = 1.0;
		
		/** Inverse cell size for fast division */
		double InvCellWidth = 1.0;
		double InvCellHeight = 1.0;

		/** Cached segment data */
		TArray<FCachedSegment> CachedSegments;

	public:
		FGridSpatialIndex() = default;

		/** Build the spatial index from a polyline */
		void Build(const FPolyline& Polyline, double PosEqualEps = 1e-5);

		/** Query all segments whose AABB overlaps the given region */
		template <typename Visitor>
		FORCEINLINE void Query(double MinX, double MinY, double MaxX, double MaxY, Visitor&& Visit) const
		{
			if (CachedSegments.IsEmpty()) return;

			/*
			const int32 NumSegments = CachedSegments.Num();
			for (int i = 0; i < NumSegments; ++i)
			{
				const FCachedSegment& Seg = CachedSegments[i];
				if (Seg.Overlaps(MinX, MinY, MaxX, MaxY))
				{
					Visit(i);
				}
			}
			*/
			// Compute cell range
			const int32 CellMinX = FMath::Clamp(static_cast<int32>((MinX - WorldMinX) * InvCellWidth), 0, GridSizeX - 1);
			const int32 CellMinY = FMath::Clamp(static_cast<int32>((MinY - WorldMinY) * InvCellHeight), 0, GridSizeY - 1);
			const int32 CellMaxX = FMath::Clamp(static_cast<int32>((MaxX - WorldMinX) * InvCellWidth), 0, GridSizeX - 1);
			const int32 CellMaxY = FMath::Clamp(static_cast<int32>((MaxY - WorldMinY) * InvCellHeight), 0, GridSizeY - 1);

			// Track visited segments to avoid duplicates (using bit array for cache efficiency)
			TBitArray<> Visited;
			Visited.Init(false, CachedSegments.Num());

			for (int32 CellY = CellMinY; CellY <= CellMaxY; ++CellY)
			{
				for (int32 CellX = CellMinX; CellX <= CellMaxX; ++CellX)
				{
					const int32 CellIdx = CellY * GridSizeX + CellX;
					for (int32 SegIdx : Cells[CellIdx])
					{
						if (!Visited[SegIdx])
						{
							Visited[SegIdx] = true;
							if (CachedSegments[SegIdx].Overlaps(MinX, MinY, MaxX, MaxY))
							{
								Visit(SegIdx);
							}
						}
					}
				}
			}
		}

		/** Get cached segment data */
		FORCEINLINE const FCachedSegment& GetSegment(int32 Index) const { return CachedSegments[Index]; }
		
		/** Get number of segments */
		FORCEINLINE int32 NumSegments() const { return CachedSegments.Num(); }

		/** Check if empty */
		FORCEINLINE bool IsEmpty() const { return CachedSegments.IsEmpty(); }
	};

	/**
	 * Pooled intersection result buffer to reduce allocations.
	 */
	struct PCGEXELEMENTSCAVALIERCONTOURS_API FIntersectionBuffer
	{
	private:
		TArray<FBasicIntersect> Buffer;
		int32 Count = 0;

	public:
		FIntersectionBuffer() { Buffer.Reserve(256); }

		FORCEINLINE void Reset() { Count = 0; }
		
		FORCEINLINE void Add(int32 Idx1, int32 Idx2, const FVector2D& Point)
		{
			if (Count >= Buffer.Num())
			{
				Buffer.AddDefaulted(FMath::Max(64, Buffer.Num()));
			}
			Buffer[Count].StartIndex1 = Idx1;
			Buffer[Count].StartIndex2 = Idx2;
			Buffer[Count].Point = Point;
			++Count;
		}

		FORCEINLINE int32 Num() const { return Count; }
		FORCEINLINE const FBasicIntersect& operator[](int32 Index) const { return Buffer[Index]; }
		FORCEINLINE FBasicIntersect& operator[](int32 Index) { return Buffer[Index]; }
		
		/** Get as array view for compatibility */
		TArrayView<const FBasicIntersect> AsArrayView() const { return TArrayView<const FBasicIntersect>(Buffer.GetData(), Count); }
	};

	/**
	 * Internal structures for self-intersection handling
	 */
	namespace Internal
	{
		/** Slice of polyline between self-intersection points */
		struct FPolylineSlice
		{
			int32 StartIndex = 0;
			int32 EndIndexOffset = 0;
			FVertex UpdatedStart;
			double UpdatedEndBulge = 0.0;
			FVector2D EndPoint = FVector2D::ZeroVector;
			FVertexSource EndSource;

			FORCEINLINE FVector2D GetStartPoint() const { return UpdatedStart.GetPosition(); }
			FORCEINLINE int32 VertexCount() const { return EndIndexOffset + 2; }
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

		/** Segment split result */
		struct FSegSplitResult
		{
			FVertex UpdatedStart;
			FVertex SplitVertex;
		};
	}

	/**
	 * Compute parallel offset of a polyline.
	 * 
	 * Positive offset values offset outward (for CCW orientation) or inward (for CW).
	 * Negative offset values offset in the opposite direction.
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
		void CreateRawOffsetSegments(
			const FPolyline& Polyline,
			double Offset,
			TArray<FRawOffsetSeg>& OutSegments);

		/** Create raw offset polyline from segments */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		FPolyline CreateRawOffsetPolyline(
			const FPolyline& OriginalPolyline,
			const TArray<FRawOffsetSeg>& Segments,
			double Offset,
			double PosEqualEps);

		/** Find all self-intersections in a polyline using grid spatial index */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		void FindAllSelfIntersections(
			const FPolyline& Polyline,
			const FGridSpatialIndex& Index,
			double PosEqualEps,
			FIntersectionBuffer& OutIntersections);

		/** Create slices using dual offset method */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		void CreateSlices(
			const FPolyline& Original,
			const FPolyline& RawOffset,
			const FPolyline& DualRawOffset,
			const FGridSpatialIndex& OrigIndex,
			double Offset,
			double PosEqualEps,
			double OffsetTolerance,
			TArray<FPolylineSlice>& OutSlices);

		/** Find intersections between two polylines */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		void FindIntersectsBetween(
			const FPolyline& Pline1,
			const FPolyline& Pline2,
			const FGridSpatialIndex& Index1,
			double PosEqualEps,
			FIntersectionBuffer& OutIntersections);

		/** Validate if a point is valid for the given offset distance */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		bool PointValidForOffset(
			const FPolyline& OriginalPolyline,
			const FGridSpatialIndex& OrigIndex,
			double Offset,
			const FVector2D& Point,
			double PosEqualEps,
			double OffsetTolerance);

		/** Stitch slices together into closed polylines */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		void StitchSlices(
			const FPolyline& RawOffsetPolyline,
			const TArray<FPolylineSlice>& Slices,
			bool bOriginalIsClosed,
			int32 SourcePathId,
			double JoinEps,
			double PosEqualEps,
			TArray<FPolyline>& OutResults);

		/** Split segment at a point (optimized version) */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		FSegSplitResult SegSplitAtPoint(
			const FVertex& V1,
			const FVertex& V2,
			const FVector2D& PointOnSeg,
			double PosEqualEps);
	}
}