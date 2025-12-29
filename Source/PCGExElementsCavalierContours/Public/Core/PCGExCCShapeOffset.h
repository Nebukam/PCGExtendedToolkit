// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours) - Shape Offset

#pragma once

#include "CoreMinimal.h"
#include "PCGExCCCommon.h"
#include "PCGExCCTypes.h"
#include "PCGExCCPolyline.h"
#include "PCGExCCMath.h"
#include "PCGExCCOffset.h"
#include "Details/PCGExCCDetails.h"

namespace PCGExCavalier::ShapeOffset
{
	/**
	 * An offset polyline with parent loop tracking.
	 * Represents a single offset result from a parent polyline, containing
	 * the generated offset polyline with its spatial index and a reference
	 * to which original input polyline it was derived from.
	 */
	struct PCGEXELEMENTSCAVALIERCONTOURS_API FOffsetLoop
	{
		/** Index of the parent loop in the original input shape */
		int32 ParentLoopIdx = INDEX_NONE;

		/** The path ID of the parent loop (for source tracking) */
		int32 ParentPathId = INDEX_NONE;

		/** The offset polyline */
		FPolyline Polyline;

		/** Spatial index for fast intersection queries */
		Offset::FGridSpatialIndex SpatialIndex;

		FOffsetLoop() = default;

		FOffsetLoop(int32 InParentIdx, int32 InPathId, FPolyline&& InPolyline)
			: ParentLoopIdx(InParentIdx)
			  , ParentPathId(InPathId)
			  , Polyline(MoveTemp(InPolyline))
		{
			SpatialIndex.Build(Polyline);
		}

		/** Get the bounding box of this loop */
		FBox2D GetBounds() const { return Polyline.BoundingBox(); }
	};

	/**
	 * Indexed polyline with spatial index for efficient queries.
	 */
	struct PCGEXELEMENTSCAVALIERCONTOURS_API FIndexedPolyline
	{
		/** The polyline geometry */
		FPolyline Polyline;

		/** Spatial index built from the polyline's segments */
		Offset::FGridSpatialIndex SpatialIndex;

		FIndexedPolyline() = default;

		explicit FIndexedPolyline(const FPolyline& InPolyline)
			: Polyline(InPolyline)
		{
			SpatialIndex.Build(Polyline);
		}

		explicit FIndexedPolyline(FPolyline&& InPolyline)
			: Polyline(MoveTemp(InPolyline))
		{
			SpatialIndex.Build(Polyline);
		}
	};

	/**
	 * Options for shape parallel offset.
	 */
	struct PCGEXELEMENTSCAVALIERCONTOURS_API FShapeOffsetOptions
	{
		/** Position equality epsilon */
		double PosEqualEps = 1e-5;

		/** Offset distance tolerance for validation */
		double OffsetDistEps = 1e-4;

		/** Slice join epsilon for stitching */
		double SliceJoinEps = 1e-4;

		FShapeOffsetOptions() = default;

		explicit FShapeOffsetOptions(const FPCGExCCOffsetOptions& OffsetOptions)
			: PosEqualEps(OffsetOptions.PositionEqualEpsilon)
			  , OffsetDistEps(OffsetOptions.OffsetDistanceEpsilon)
			  , SliceJoinEps(OffsetOptions.SliceJoinEpsilon)
		{
		}
	};

	/**
	 * Intersection data between two offset loops.
	 */
	struct PCGEXELEMENTSCAVALIERCONTOURS_API FSlicePointSet
	{
		/** Index of the first offset loop in the intersection pair */
		int32 LoopIdx1 = INDEX_NONE;

		/** Index of the second offset loop in the intersection pair */
		int32 LoopIdx2 = INDEX_NONE;

		/** All intersection points between the two loops */
		TArray<FBasicIntersect> SlicePoints;
	};

	/**
	 * A validated slice of an offset polyline ready for stitching.
	 */
	struct PCGEXELEMENTSCAVALIERCONTOURS_API FDissectedSlice
	{
		/** Index of the source offset loop this slice comes from */
		int32 SourceIdx = INDEX_NONE;

		/** Start index in the source polyline */
		int32 StartIndex = 0;

		/** End index offset (number of vertices from start, wrapping) */
		int32 EndIndexOffset = 0;

		/** Updated start vertex */
		FVertex UpdatedStart;

		/** Updated end bulge */
		double UpdatedEndBulge = 0.0;

		/** End point position */
		FVector2D EndPoint = FVector2D::ZeroVector;

		/** Source information for end vertex */
		FVertexSource EndSource;

		/** Get start point */
		FVector2D GetStartPoint() const { return UpdatedStart.GetPosition(); }
	};

	/**
	 * Shape represented by positive area (CCW) and negative/hole area (CW) polylines.
	 * Used for multi-polyline parallel offset operations.
	 */
	class PCGEXELEMENTSCAVALIERCONTOURS_API FShape
	{
	public:
		/** Positive/filled area counter-clockwise polylines */
		TArray<FIndexedPolyline> CCWPolylines;

		/** Negative/hole area clockwise polylines */
		TArray<FIndexedPolyline> CWPolylines;

		/** Path IDs for CCW polylines */
		TArray<int32> CCWPathIds;

		/** Path IDs for CW polylines */
		TArray<int32> CWPathIds;

		FShape() = default;

		static FShape FromPolylines(const TArray<FPolyline>& Polylines);

		/** Create an empty shape */
		static FShape Empty() { return FShape(); }

		/** Check if shape is empty */
		bool IsEmpty() const { return CCWPolylines.IsEmpty() && CWPolylines.IsEmpty(); }

		/** Get total number of polylines */
		int32 Num() const { return CCWPolylines.Num() + CWPolylines.Num(); }

		/**
		 * Perform parallel offset on the entire shape.
		 * Handles interactions between multiple polylines (outer boundaries and holes).
		 * 
		 * @param Offset The offset distance (positive = outward for CCW, inward for CW)
		 * @param Options Offset options
		 * @return Resulting shape after offset
		 */
		FShape ParallelOffset(double Offset, const FShapeOffsetOptions& Options) const;

		/**
		 * Get all result polylines from the shape.
		 */
		TArray<FPolyline> GetAllPolylines() const;

	private:
		/**
		 * Step 1: Create offset loops with spatial index.
		 * Generates offset polylines for each input polyline and creates spatial indices.
		 */
		void CreateOffsetLoopsWithIndex(
			double Offset,
			const FShapeOffsetOptions& Options,
			TArray<FOffsetLoop>& OutCCWOffsetLoops,
			TArray<FOffsetLoop>& OutCWOffsetLoops) const;

		/**
		 * Step 2: Find intersections between offset loops.
		 */
		void FindIntersectsBetweenOffsetLoops(
			const TArray<FOffsetLoop>& CCWOffsetLoops,
			const TArray<FOffsetLoop>& CWOffsetLoops,
			double PosEqualEps,
			TArray<FSlicePointSet>& OutSlicePointSets) const;

		/**
		 * Step 3: Create valid slices from intersection points.
		 */
		void CreateValidSlicesFromIntersects(
			const TArray<FOffsetLoop>& CCWOffsetLoops,
			const TArray<FOffsetLoop>& CWOffsetLoops,
			const TArray<FSlicePointSet>& SlicePointSets,
			double Offset,
			const FShapeOffsetOptions& Options,
			TArray<FDissectedSlice>& OutSlicesData) const;

		/**
		 * Step 4: Stitch slices together into final polylines.
		 */
		FShape StitchSlicesTogether(
			TArray<FDissectedSlice>& SlicesData,
			const TArray<FOffsetLoop>& CCWOffsetLoops,
			const TArray<FOffsetLoop>& CWOffsetLoops,
			double PosEqualEps,
			double SliceJoinEps) const;

		/** Helper to get a loop by combined index */
		static const FOffsetLoop& GetLoop(
			int32 Index,
			const TArray<FOffsetLoop>& CCWLoops,
			const TArray<FOffsetLoop>& CWLoops);

		/** Helper to get indexed polyline by combined index */
		const FIndexedPolyline& GetIndexedPolyline(int32 Index) const;

		/** Helper to get path ID by combined index */
		int32 GetPathId(int32 Index) const;

		/** Validate if a point is valid for the given offset distance against all input polylines */
		bool IsSliceValid(
			const FDissectedSlice& Slice,
			const FOffsetLoop& OffsetLoop,
			double Offset,
			const FShapeOffsetOptions& Options) const;
	};

	/**
	 * Perform parallel offset on a set of polylines that form a shape (outer boundaries with holes).
	 * 
	 * @param Polylines Input polylines (CCW = outer, CW = holes)
	 * @param Offset The offset distance
	 * @param Options Offset options
	 * @return Array of resulting offset polylines
	 */
	PCGEXELEMENTSCAVALIERCONTOURS_API
	TArray<FPolyline> ParallelOffsetShape(const TArray<FPolyline>& Polylines, double Offset, const FShapeOffsetOptions& Options);

	/**
	 * Perform parallel offset on a set of polylines (simpler interface).
	 */
	PCGEXELEMENTSCAVALIERCONTOURS_API
	TArray<FPolyline> ParallelOffsetShape(const TArray<FPolyline>& Polylines, double Offset, const FPCGExCCOffsetOptions& Options = FPCGExCCOffsetOptions());
}
