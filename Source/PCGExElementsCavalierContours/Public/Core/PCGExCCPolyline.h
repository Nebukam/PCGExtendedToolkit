// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#pragma once

#include "CoreMinimal.h"
#include "PCGExCCTypes.h"
#include "PCGExCCMath.h"
#include "Details/PCGExCCDetails.h"
#include "Utils/PCGValueRange.h"

/**
 * A 2D polyline that can represent both straight line segments and circular arc segments.
 * 
 * The polyline stores a sequence of vertices, each with a position (X, Y) and a bulge value.
 * The bulge value defines the curvature of the segment starting at that vertex:
 * - 0 = straight line to next vertex
 * - positive = counter-clockwise arc
 * - negative = clockwise arc
 * - bulge = tan(sweep_angle / 4)
 * 
 * Can be either open (start and end don't connect) or closed (forms a loop).
 */
namespace PCGExCavalier
{
	class PCGEXELEMENTSCAVALIERCONTOURS_API FPolyline
	{
	public:
		//~ Constructors

		FPolyline() = default;

		explicit FPolyline(bool bInIsClosed)
			: bIsClosed(bInIsClosed)
		{
		}

		FPolyline(TArray<FVertex> InVertices, bool bInIsClosed)
			: Vertices(MoveTemp(InVertices))
			  , bIsClosed(bInIsClosed)
		{
		}

		//~ Factory methods

		/** Create from array of 2D points (all line segments) */
		static FPolyline FromPoints(const TArray<FVector2D>& Points, bool bClosed);

		/** Create from array of 3D vectors (XY used, Z ignored) */
		static FPolyline FromVectors(const TArray<FVector>& Vectors, bool bClosed);

		/** Create from array of transforms (XY of location used) */
		static FPolyline FromTransforms(const TConstPCGValueRange<FTransform>& Transforms, bool bClosed);

		/** Create from input points with corner processing */
		static FPolyline FromInputPoints(const TArray<FInputPoint>& Points, bool bClosed);

		//~ Basic accessors

		/** Get number of vertices */
		int32 VertexCount() const { return Vertices.Num(); }

		/** Get number of segments */
		int32 SegmentCount() const;

		/** Check if polyline is empty */
		bool IsEmpty() const { return Vertices.Num() == 0; }

		/** Check if polyline is closed */
		bool IsClosed() const { return bIsClosed; }

		/** Set whether polyline is closed */
		void SetIsClosed(bool bNewClosed) { bIsClosed = bNewClosed; }

		/** Get vertex at index */
		const FVertex& GetVertex(int32 Index) const { return Vertices[Index]; }

		/** Get vertex at index (mutable) */
		FVertex& GetVertex(int32 Index) { return Vertices[Index]; }

		/** Get vertex with wrapping index */
		const FVertex& GetVertexWrapped(int32 Index) const;

		/** Get all vertices */
		const TArray<FVertex>& GetVertices() const { return Vertices; }

		/** Get all vertices (mutable) */
		TArray<FVertex>& GetVertices() { return Vertices; }

		/** Get first vertex (asserts non-empty) */
		const FVertex& GetFirst() const
		{
			check(!IsEmpty());
			return Vertices[0];
		}

		/** Get last vertex (asserts non-empty) */
		const FVertex& GetLast() const
		{
			check(!IsEmpty());
			return Vertices.Last();
		}

		//~ Vertex manipulation

		/** Add a vertex */
		void AddVertex(const FVertex& Vertex);
		void AddVertex(double X, double Y, double Bulge = 0.0);

		/** Add vertex, replacing last if position matches */
		void AddOrReplaceVertex(const FVertex& Vertex, double PosEqualEps = 1e-5);

		/** Set the last vertex */
		void SetLastVertex(const FVertex& Vertex);

		/** Remove and return the last vertex */
		FVertex RemoveLastVertex();

		/** Clear all vertices */
		void Clear();

		/** Reserve capacity */
		void Reserve(int32 Capacity) { Vertices.Reserve(Capacity); }

		//~ Index utilities

		/** Get next wrapping index */
		int32 NextWrappingIndex(int32 Index) const;

		/** Get previous wrapping index */
		int32 PrevWrappingIndex(int32 Index) const;

		//~ Geometric properties

		/** Calculate total path length */
		double PathLength() const;

		/** Calculate signed area (positive for CCW, negative for CW) */
		double Area() const;

		/** Get orientation */
		EPCGExCCOrientation Orientation() const;

		/** Get axis-aligned bounding box. Returns false if polyline has < 2 vertices */
		bool GetExtents(FBox2D& OutBox) const;

		//~ Segment iteration

		/** Visitor callback for segment iteration */
		using FSegmentVisitor = TFunctionRef<void(const FVertex& V1, const FVertex& V2)>;

		/** Visit each segment */
		void ForEachSegment(FSegmentVisitor Visitor) const;

		//~ Transformations

		/** Create copy with direction inverted */
		FPolyline Inverted() const;

		/** Invert direction in place */
		void Invert();

		/** Create copy with redundant vertices removed */
		FPolyline WithRedundantVerticesRemoved(double PosEqualEps = 1e-5) const;

		/** Remove redundant vertices in place. Returns true if any were removed */
		bool RemoveRedundantVertices(double PosEqualEps = 1e-5);

		//~ Arc tessellation

		/**
		 * Tessellate all arcs to line segments.
		 * @param Settings Tessellation settings
		 * @return New polyline with only line segments
		 */
		FPolyline Tessellated(const FPCGExCCArcTessellationSettings& Settings) const;

		/**
		 * Tessellate arcs in place.
		 * @param Settings Tessellation settings
		 */
		void Tessellate(const FPCGExCCArcTessellationSettings& Settings);

		//~ Point containment

		/**
		 * Calculate winding number for a point (for closed polylines).
		 * @param Point Point to test
		 * @return Winding number (0 = outside, non-zero = inside)
		 */
		int32 WindingNumber(const FVector2D& Point) const;

		/**
		 * Test if point is inside this closed polyline.
		 * @param Point Point to test
		 * @return True if point is inside
		 */
		bool ContainsPoint(const FVector2D& Point) const;

		//~ Closest point

		/**
		 * Find closest point on polyline to given point.
		 * @param Point Point to find closest to
		 * @param OutSegmentIndex Optional output of segment index
		 * @param OutDistance Optional output of distance
		 * @return Closest point on polyline
		 */
		FVector2D ClosestPoint(const FVector2D& Point, int32* OutSegmentIndex = nullptr, double* OutDistance = nullptr) const;

		//~ Comparison

		/** Fuzzy equality comparison */
		bool FuzzyEquals(const FPolyline& Other, double Epsilon = 1e-9) const;

	private:
		TArray<FVertex> Vertices;
		bool bIsClosed = false;
	};

	/**
	 * Result structure for converting polylines to 3D with Z interpolation
	 */
	struct PCGEXELEMENTSCAVALIERCONTOURS_API FContourResult3D
	{
		/** Output positions */
		TArray<FVector> Positions;

		/** Output transforms (with identity rotation/scale) */
		TArray<FTransform> Transforms;

		/** Is this contour closed? */
		bool bIsClosed = false;
	};

	/**
	 * Utility class for working with contours and converting between 2D/3D
	 */
	class PCGEXELEMENTSCAVALIERCONTOURS_API FContourUtils
	{
	public:
		/**
		 * Convert a 2D polyline to 3D with Z interpolation from source points.
		 * @param Polyline2D The tessellated 2D polyline
		 * @param SourcePoints Original input points with Z values
		 * @param bClosed Whether the original contour was closed
		 * @return 3D result with positions and transforms
		 */
		static FContourResult3D ConvertTo3D(
			const FPolyline& Polyline2D,
			const TArray<FInputPoint>& SourcePoints,
			bool bClosed);

		/**
		 * Convert polyline to array of transforms with Z interpolation.
		 * @param Polyline2D The tessellated 2D polyline
		 * @param SourceZValues Array of Z values corresponding to original vertices
		 * @param bClosed Whether the original contour was closed
		 * @return Array of transforms
		 */
		static TArray<FTransform> ToTransforms(
			const FPolyline& Polyline2D,
			const TArray<double>& SourceZValues,
			bool bClosed);

		/**
		 * Process input points to create rounded corners where marked.
		 * @param Points Input points with corner flags
		 * @param bClosed Whether the contour is closed
		 * @return Polyline with corners converted to arcs
		 */
		static FPolyline ProcessCorners(
			const TArray<FInputPoint>& Points,
			bool bClosed);

		/**
		 * Compute parallel offsets and tessellate results.
		 * @param InputPoints Source points
		 * @param bClosed Whether closed
		 * @param OffsetDistance Offset distance
		 * @param TessellationSettings Arc tessellation settings
		 * @param OffsetOptions Offset algorithm options
		 * @return Array of 3D contour results
		 */
		static TArray<FContourResult3D> ComputeOffsetContours(
			const TArray<FInputPoint>& InputPoints,
			bool bClosed,
			double OffsetDistance,
			const FPCGExCCArcTessellationSettings& TessellationSettings,
			const FPCGExCCOffsetOptions& OffsetOptions = FPCGExCCOffsetOptions());
	};
}
