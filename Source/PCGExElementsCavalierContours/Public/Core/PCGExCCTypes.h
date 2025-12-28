// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#pragma once

#include "CoreMinimal.h"
#include "PCGExCCTypes.generated.h"

struct FPCGExGeo2DProjectionDetails;

namespace PCGExData
{
	class FFacade;
	class FPointIO;
}

/**
 * Arc tessellation mode for converting arcs to line segments
 */
UENUM(BlueprintType)
enum class EPCGExCCArcTessellationMode : uint8
{
	FixedCount UMETA(DisplayName = "Fixed Count", ToolTip = "Fixed number of subdivisions per arc"),
	DistanceBased UMETA(DisplayName = "Distance Based", ToolTip = "Compute subdivisions based on arc length and target segment distance"),
};

/**
 * Orientation of a closed polyline
 */
UENUM(BlueprintType)
enum class EPCGExCCOrientation : uint8
{
	Open UMETA(DisplayName = "Open", ToolTip="Open path"),
	Clockwise UMETA(DisplayName = "Clockwise", ToolTip="Closed & Clockwise path"),
	CounterClockwise UMETA(DisplayName = "Counter Clockwise", ToolTip="Closed & Clockwise path"),
};

/**
 * Boolean operation types for polyline operations
 */
UENUM(BlueprintType)
enum class EPCGExCCBooleanOp : uint8
{
	Union        = 0 UMETA(DisplayName = "Union", ToolTip = "Union of the paths"),
	Intersection = 1 UMETA(DisplayName = "Intersection", ToolTip = "Intersection of the paths"),
	Difference   = 2 UMETA(DisplayName = "Difference", ToolTip = "Difference (substraction) of the paths"),
	Xor          = 3 UMETA(DisplayName = "XOR", ToolTip = "Exclusive OR between paths")
};

namespace PCGExCavalier
{
	/**
	 * Tracks the origin of a vertex back to its root path and point.
	 * Used for mapping output vertices back to their source data after
	 * operations like offset, tessellation, and boolean ops.
	 */
	struct PCGEXELEMENTSCAVALIERCONTOURS_API FVertexSource
	{
		/** 
		 * Unique identifier of the root path this vertex originated from.
		 * Each input path should have a unique PathId assigned by the caller.
		 */
		int32 PathId = INDEX_NONE;

		/** 
		 * Index of the point within the root path.
		 * This corresponds to the FInputPoint's position in its source array.
		 */
		int32 PointIndex = INDEX_NONE;

		FVertexSource() = default;

		FVertexSource(int32 InPathId, int32 InPointIndex)
			: PathId(InPathId)
			  , PointIndex(InPointIndex)
		{
		}

		/** Returns true if both PathId and PointIndex are valid */
		bool IsValid() const
		{
			return PathId != INDEX_NONE && PointIndex != INDEX_NONE;
		}

		/** Returns true if PathId is valid (vertex belongs to a known path) */
		bool HasValidPath() const
		{
			return PathId != INDEX_NONE;
		}

		/** Returns true if PointIndex is valid (can be traced to specific point) */
		bool HasValidPoint() const
		{
			return PointIndex != INDEX_NONE;
		}

		/** Create an invalid source */
		static FVertexSource Invalid()
		{
			return FVertexSource();
		}

		/** Create a source with only PathId (point unknown) */
		static FVertexSource FromPath(int32 InPathId)
		{
			return FVertexSource(InPathId, INDEX_NONE);
		}

		bool operator==(const FVertexSource& Other) const
		{
			return PathId == Other.PathId && PointIndex == Other.PointIndex;
		}

		bool operator!=(const FVertexSource& Other) const
		{
			return !(*this == Other);
		}
	};


	/**
	 * Input point for contour operations with optional corner flag and radius.
	 * Stores the original transform for proper 3D reconstruction after 2D operations.
	 */
	struct PCGEXELEMENTSCAVALIERCONTOURS_API FInputPoint
	{
		/** Unique identifier of the root path this point belongs to */
		int32 PathId = INDEX_NONE;

		/** Index within the root path's point array */
		int32 PointIndex = INDEX_NONE;

		/** Original transform (full 3D information for reconstruction) */
		FTransform Transform = FTransform::Identity;

		/** Whether this point should be treated as a corner (for filleting/rounding) */
		bool bIsCorner = false;

		/** Radius for corner rounding (only used if bIsCorner is true) */
		double CornerRadius = 0.0;

		FInputPoint() = default;

		/** Construct with path and point index, position only */
		FInputPoint(int32 InPathId, int32 InPointIndex, const FVector& InPosition, bool InIsCorner = false, double InRadius = 0.0)
			: PathId(InPathId)
			  , PointIndex(InPointIndex)
			  , Transform(FTransform(InPosition))
			  , bIsCorner(InIsCorner)
			  , CornerRadius(InRadius)
		{
		}

		/** Construct with path and point index, full transform */
		FInputPoint(int32 InPathId, int32 InPointIndex, const FTransform& InTransform, bool InIsCorner = false, double InRadius = 0.0)
			: PathId(InPathId)
			  , PointIndex(InPointIndex)
			  , Transform(InTransform)
			  , bIsCorner(InIsCorner)
			  , CornerRadius(InRadius)
		{
		}

		/** Legacy constructor - uses SourceIndex as PointIndex, default PathId = 0 */
		explicit FInputPoint(int32 InSourceIndex, const FVector& InPosition, bool InIsCorner = false, double InRadius = 0.0)
			: PathId(0) // Default to path 0 for legacy code
			  , PointIndex(InSourceIndex)
			  , Transform(FTransform(InPosition))
			  , bIsCorner(InIsCorner)
			  , CornerRadius(InRadius)
		{
		}

		/** Get the vertex source for creating FVertex from this input point */
		FVertexSource GetSource() const
		{
			return FVertexSource(PathId, PointIndex);
		}

		/** Get 3D position from transform */
		FVector GetPosition() const { return Transform.GetLocation(); }

		/** Get 2D position (XY) */
		FVector2D GetPosition2D() const
		{
			const FVector Loc = Transform.GetLocation();
			return FVector2D(Loc.X, Loc.Y);
		}

		/** Get Z value */
		double GetZ() const { return Transform.GetLocation().Z; }
	};


	/**
	 * A 2D polyline vertex with position and bulge value.
	 * 
	 * The bulge value determines the curvature of the segment from this vertex to the next:
	 * - A bulge of 0.0 creates a straight line segment
	 * - A positive bulge creates a counter-clockwise arc
	 * - A negative bulge creates a clockwise arc
	 * - bulge = tan(arc_sweep_angle / 4)
	 * 
	 * Note: Bulge values are limited to [-1, 1] which corresponds to arcs up to a half-circle.
	 *
	 * The Source tracks which original root path and input point this vertex derives from:
	 * - For vertices created directly from input points: Source = {PathId, PointIndex}
	 * - For tessellated vertices (intermediate arc points): inherits Source from arc start vertex
	 * - For offset vertices: inherits Source from the corresponding source vertex
	 * - For boolean operation results: inherits Source from whichever source polyline contributed the vertex
	 * - Invalid source indicates completely synthetic vertices (e.g., intersection points)
	 */
	struct PCGEXELEMENTSCAVALIERCONTOURS_API FVertex
	{
		/** 2D position */
		FVector2D Position = FVector2D::ZeroVector;

		/** Bulge value for arc segments */
		double Bulge = 0.0;

		/** 
		 * Source tracking back to the root path and point.
		 * Used for proper 3D reconstruction and metadata propagation.
		 */
		FVertexSource Source;

		FVertex() = default;

		FVertex(double InX, double InY, double InBulge = 0.0)
			: Position(InX, InY)
			  , Bulge(InBulge)
			  , Source()
		{
		}

		FVertex(double InX, double InY, double InBulge, const FVertexSource& InSource)
			: Position(InX, InY)
			  , Bulge(InBulge)
			  , Source(InSource)
		{
		}

		FVertex(double InX, double InY, double InBulge, int32 InPathId, int32 InPointIndex)
			: Position(InX, InY)
			  , Bulge(InBulge)
			  , Source(InPathId, InPointIndex)
		{
		}

		FVertex(const FVector2D& InPosition, double InBulge = 0.0)
			: Position(InPosition)
			  , Bulge(InBulge)
			  , Source()
		{
		}

		FVertex(const FVector2D& InPosition, double InBulge, const FVertexSource& InSource)
			: Position(InPosition)
			  , Bulge(InBulge)
			  , Source(InSource)
		{
		}

		FVertex(const FVector2D& InPosition, double InBulge, int32 InPathId, int32 InPointIndex)
			: Position(InPosition)
			  , Bulge(InBulge)
			  , Source(InPathId, InPointIndex)
		{
		}

		/** Get X coordinate */
		double GetX() const { return Position.X; }

		/** Get Y coordinate */
		double GetY() const { return Position.Y; }

		/** Get 2D position */
		const FVector2D& GetPosition() const { return Position; }

		/** Set 2D position */
		void SetPosition(const FVector2D& InPosition) { Position = InPosition; }

		/** Set position from X, Y */
		void SetPosition(double X, double Y) { Position = FVector2D(X, Y); }

		/** Returns true if this vertex starts a line segment (bulge is approximately zero) */
		bool IsLine(double Epsilon = 1e-9) const { return FMath::Abs(Bulge) < Epsilon; }

		/** Returns true if this vertex starts an arc segment (bulge is non-zero) */
		bool IsArc(double Epsilon = 1e-9) const { return !IsLine(Epsilon); }

		/** Returns true if this vertex starts a counter-clockwise arc */
		bool IsArcCCW() const { return Bulge > 0.0; }

		/** Returns true if this vertex starts a clockwise arc */
		bool IsArcCW() const { return Bulge < 0.0; }

		/** Returns true if this vertex has a fully valid source (path and point) */
		bool HasValidSource() const { return Source.IsValid(); }

		/** Returns true if this vertex has a valid path (even if point is unknown) */
		bool HasValidPath() const { return Source.HasValidPath(); }

		/** Get the path ID this vertex belongs to */
		int32 GetPathId() const { return Source.PathId; }

		/** Get the point index within the path */
		int32 GetPointIndex() const { return Source.PointIndex; }

		/** Create a copy with a different bulge value */
		FVertex WithBulge(double NewBulge) const
		{
			return FVertex(Position, NewBulge, Source);
		}

		/** Create a copy with a different source */
		FVertex WithSource(const FVertexSource& NewSource) const
		{
			return FVertex(Position, Bulge, NewSource);
		}

		/** Create a copy with a different path ID (preserves point index) */
		FVertex WithPathId(int32 NewPathId) const
		{
			return FVertex(Position, Bulge, FVertexSource(NewPathId, Source.PointIndex));
		}

		/** Create a copy with a different point index (preserves path ID) */
		FVertex WithPointIndex(int32 NewPointIndex) const
		{
			return FVertex(Position, Bulge, FVertexSource(Source.PathId, NewPointIndex));
		}

		/** Fuzzy equality comparison (position and bulge only, not source) */
		bool FuzzyEquals(const FVertex& Other, double Epsilon = 1e-9) const
		{
			return Position.Equals(Other.Position, Epsilon) &&
				FMath::Abs(Bulge - Other.Bulge) < Epsilon;
		}

		/** Position-only fuzzy equality */
		bool PositionEquals(const FVertex& Other, double Epsilon = 1e-9) const
		{
			return Position.Equals(Other.Position, Epsilon);
		}

		/** Position-only fuzzy equality with FVector2D */
		bool PositionEquals(const FVector2D& OtherPos, double Epsilon = 1e-9) const
		{
			return Position.Equals(OtherPos, Epsilon);
		}


		// Legacy compatibility - RootIndex as alias for PointIndex


		/** @deprecated Use Source.PointIndex or GetPointIndex() instead */
		int32 GetRootIndex() const { return Source.PointIndex; }

		/** @deprecated Use HasValidSource() or Source.IsValid() instead */
		bool HasValidRootIndex() const { return Source.PointIndex != INDEX_NONE; }

		/** @deprecated Use WithSource() or WithPointIndex() instead */
		FVertex WithRootIndex(int32 NewRootIndex) const
		{
			return FVertex(Position, Bulge, FVertexSource(Source.PathId, NewRootIndex));
		}
	};


	/**
	 * A collection of input points forming a single root path.
	 * Each root path has a unique identifier for tracking through operations.
	 */
	struct PCGEXELEMENTSCAVALIERCONTOURS_API FRootPath
	{
		/** Unique identifier for this path */
		int32 PathId = INDEX_NONE;

		/** The points that make up this path */
		TArray<FInputPoint> Points;

		/** Whether this path is closed */
		bool bIsClosed = true;

		TSharedPtr<PCGExData::FFacade> PathFacade = nullptr;

		FRootPath() = default;
		FRootPath(const int32 InPathId, const TSharedPtr<PCGExData::FFacade>& InFacade, const FPCGExGeo2DProjectionDetails& InProjectionDetails);

		/** Add a point to this path, automatically setting its PathId and PointIndex */
		void AddPoint(const FVector& Position, bool bIsCorner = false, double CornerRadius = 0.0)
		{
			const int32 PointIndex = Points.Num();
			Points.Add(FInputPoint(PathId, PointIndex, Position, bIsCorner, CornerRadius));
		}

		/** Add a point to this path with full transform */
		void AddPoint(const FTransform& Transform, bool bIsCorner = false, double CornerRadius = 0.0)
		{
			const int32 PointIndex = Points.Num();
			Points.Add(FInputPoint(PathId, PointIndex, Transform, bIsCorner, CornerRadius));
		}

		/** Reserve capacity for points */
		void Reserve(int32 Count) { Points.Reserve(Count); }

		/** Get number of points */
		int32 Num() const { return Points.Num(); }

		/** Get point at index */
		const FInputPoint& operator[](int32 Index) const { return Points[Index]; }
		FInputPoint& operator[](int32 Index) { return Points[Index]; }

		/** Check if empty */
		bool IsEmpty() const { return Points.IsEmpty(); }
	};
}
