// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#pragma once

#include "CoreMinimal.h"
#include "PCGExCCTypes.generated.h"

/**
 * Arc tessellation mode for converting arcs to line segments
 */
UENUM(BlueprintType)
enum class EPCGExCCArcTessellationMode : uint8
{
	/** Fixed number of subdivisions per arc */
	FixedCount UMETA(DisplayName = "Fixed Count"),

	/** Compute subdivisions based on arc length and target segment distance */
	DistanceBased UMETA(DisplayName = "Distance Based")
};

/**
 * Orientation of a closed polyline
 */
UENUM(BlueprintType)
enum class EPCGExCCOrientation : uint8
{
	/** Polyline is open */
	Open UMETA(DisplayName = "Open"),

	/** Polyline is closed and directionally clockwise */
	Clockwise UMETA(DisplayName = "Clockwise"),

	/** Polyline is closed and directionally counter-clockwise */
	CounterClockwise UMETA(DisplayName = "Counter Clockwise")
};

/**
 * Boolean operation types for polyline operations
 */
UENUM(BlueprintType)
enum class EPCGExCCBooleanOp : uint8
{
	/** Union of the polylines */
	Union UMETA(DisplayName = "Union"),

	/** Intersection of the polylines */
	Intersection UMETA(DisplayName = "Intersection"),

	/** Difference (subtraction) of polylines */
	Difference UMETA(DisplayName = "Difference"),

	/** Exclusive OR between polylines */
	Xor UMETA(DisplayName = "XOR")
};

namespace PCGExCavalier
{
	/**
 * Input point for contour operations with optional corner flag and radius
 */
	USTRUCT(BlueprintType)
	struct FInputPoint
	{
		/** 3D Position (Z will be used for extrapolation) */
		FVector Position = FVector::ZeroVector;

		/** Whether this point should be treated as a corner (for filleting/rounding) */
		bool bIsCorner = false;

		/** Radius for corner rounding (only used if bIsCorner is true) */
		double CornerRadius = 0.0;

		FInputPoint() = default;

		FInputPoint(const FVector& InPosition, bool InIsCorner = false, double InRadius = 0.0)
			: Position(InPosition), bIsCorner(InIsCorner), CornerRadius(InRadius)
		{
		}

		explicit FInputPoint(const FTransform& Transform, bool InIsCorner = false, double InRadius = 0.0)
			: Position(Transform.GetLocation()), bIsCorner(InIsCorner), CornerRadius(InRadius)
		{
		}
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
	 */
	struct PCGEXELEMENTSCAVALIERCONTOURS_API FVertex
	{
		double X = 0.0;
		double Y = 0.0;
		double Bulge = 0.0;

		FVertex() = default;

		FVertex(double InX, double InY, double InBulge = 0.0)
			: X(InX), Y(InY), Bulge(InBulge)
		{
		}

		explicit FVertex(const FVector2D& Position, double InBulge = 0.0)
			: X(Position.X), Y(Position.Y), Bulge(InBulge)
		{
		}

		/** Get 2D position */
		FVector2D GetPosition() const { return FVector2D(X, Y); }

		/** Set 2D position */
		void SetPosition(const FVector2D& Position)
		{
			X = Position.X;
			Y = Position.Y;
		}

		/** Returns true if this vertex starts a line segment (bulge is approximately zero) */
		bool IsLine(double Epsilon = 1e-9) const { return FMath::Abs(Bulge) < Epsilon; }

		/** Returns true if this vertex starts a counter-clockwise arc */
		bool IsArcCCW() const { return Bulge > 0.0; }

		/** Returns true if this vertex starts a clockwise arc */
		bool IsArcCW() const { return Bulge < 0.0; }

		/** Fuzzy equality comparison */
		bool FuzzyEquals(const FVertex& Other, double Epsilon = 1e-9) const
		{
			return FMath::Abs(X - Other.X) < Epsilon &&
				FMath::Abs(Y - Other.Y) < Epsilon &&
				FMath::Abs(Bulge - Other.Bulge) < Epsilon;
		}
	};
}
