// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#pragma once

#include "CoreMinimal.h"
#include "PCGExCCTypes.h"

namespace PCGExCavalier
{
	/**
	 * Core math utilities for 2D contour operations
	 */
	namespace Math
	{
		/** Default fuzzy epsilon for comparisons */
		constexpr double FuzzyEpsilon = UE_SMALL_NUMBER;

		//~ Angle utilities

		/**
		 * Normalize radians to be between 0 and 2*PI
		 */
		FORCEINLINE double NormalizeRadians(double Angle)
		{
			if (Angle >= 0.0 && Angle <= UE_TWO_PI) { return Angle; }
			return Angle - FMath::Floor(Angle / UE_TWO_PI) * UE_TWO_PI;
		}

		/**
		 * Returns the smaller difference between two angles.
		 * Result is negative if normalize_radians(angle2 - angle1) > PI
		 */
		FORCEINLINE double DeltaAngle(double Angle1, double Angle2)
		{
			double Diff = NormalizeRadians(Angle2 - Angle1);
			if (Diff > UE_PI) { Diff = Diff - UE_TWO_PI; }
			return Diff;
		}

		/**
		 * Returns the delta angle with a specific sign applied
		 */
		FORCEINLINE double DeltaAngleSigned(double Angle1, double Angle2, bool bNegative)
		{
			const double Diff = DeltaAngle(Angle1, Angle2);
			return bNegative ? -FMath::Abs(Diff) : FMath::Abs(Diff);
		}

		/**
		 * Tests if test_angle is between start_angle and end_angle (counter-clockwise sweep)
		 */
		FORCEINLINE bool AngleIsBetween(double TestAngle, double StartAngle, double EndAngle, double Epsilon = FuzzyEpsilon)
		{
			const double EndSweep = NormalizeRadians(EndAngle - StartAngle);
			const double MidSweep = NormalizeRadians(TestAngle - StartAngle);
			return MidSweep < EndSweep + Epsilon;
		}

		/**
		 * Tests if test_angle is within the sweep_angle starting at start_angle
		 */
		FORCEINLINE bool AngleIsWithinSweep(double TestAngle, double StartAngle, double SweepAngle, double Epsilon = FuzzyEpsilon)
		{
			const double EndAngle = StartAngle + SweepAngle;
			if (SweepAngle < 0.0) { return AngleIsBetween(TestAngle, EndAngle, StartAngle, Epsilon); }
			return AngleIsBetween(TestAngle, StartAngle, EndAngle, Epsilon);
		}

		//~ Bulge/Arc utilities

		/**
		 * Convert arc sweep angle to bulge value
		 * bulge = tan(sweep_angle / 4)
		 */
		FORCEINLINE double BulgeFromAngle(double SweepAngle)
		{
			return FMath::Tan(SweepAngle / 4.0);
		}

		/**
		 * Convert bulge value to arc sweep angle
		 * sweep_angle = 4 * atan(bulge)
		 */
		FORCEINLINE double AngleFromBulge(double Bulge)
		{
			return 4.0 * FMath::Atan(Bulge);
		}

		//~ Point/Vector utilities

		/**
		 * Get angle of direction vector from P0 to P1
		 */
		FORCEINLINE double Angle(const FVector2D& P0, const FVector2D& P1)
		{
			return FMath::Atan2(P1.Y - P0.Y, P1.X - P0.X);
		}

		/**
		 * Squared distance between two points
		 */
		FORCEINLINE double DistanceSquared(const FVector2D& P0, const FVector2D& P1)
		{
			const FVector2D D = P0 - P1;
			return FVector2D::DotProduct(D, D);
		}

		/**
		 * Get midpoint between two points
		 */
		FORCEINLINE FVector2D Midpoint(const FVector2D& P0, const FVector2D& P1)
		{
			return FVector2D((P0.X + P1.X) / 2.0, (P0.Y + P1.Y) / 2.0);
		}

		/** 
		 * Calculates the midpoint of an arc defined by two points and a bulge. 
		 * 
		 */
		FORCEINLINE FVector2D ArcMidpoint(const FVector2D& P1, const FVector2D& P2, double Bulge)
		{
			// 1. Linear midpoint for straight lines (or near-zero arcs)
			const FVector2D ChordMid = (P1 + P2) * 0.5;
			if (FMath::IsNearlyZero(Bulge, 1e-8)) { return ChordMid; }

			const FVector2D Chord = P2 - P1;
			const double Dist = Chord.Size();
			if (FMath::IsNearlyZero(Dist)) { return P1; }

			// 2. Perpendicular vector to the chord
			// Positive bulge curves to the left of the direction P1 -> P2
			const FVector2D Perp = FVector2D(-Chord.Y, Chord.X) / Dist;

			// 3. Sagitta (distance from chord midpoint to arc midpoint)
			// Formula: s = (ChordLength / 2) * Bulge
			const double Sagitta = (Dist * 0.5) * Bulge;

			return ChordMid + (Perp * Sagitta);
		}

		/**
		 * Point on circle at given angle
		 */
		FORCEINLINE FVector2D PointOnCircle(double Radius, const FVector2D& Center, double AngleRadians)
		{
			double S, C;
			FMath::SinCos(&S, &C, AngleRadians);
			return FVector2D(Center.X + Radius * C, Center.Y + Radius * S);
		}

		/**
		 * Point from parametric value on line segment
		 */
		FORCEINLINE FVector2D PointFromParametric(const FVector2D& P0, const FVector2D& P1, double T)
		{
			return P0 + (P1 - P0) * T;
		}

		/**
		 * Perpendicular vector (rotated 90 degrees CCW)
		 */
		FORCEINLINE FVector2D Perp(const FVector2D& V)
		{
			return FVector2D(-V.Y, V.X);
		}

		/**
		 * Unit perpendicular vector
		 */
		FORCEINLINE FVector2D UnitPerp(const FVector2D& V)
		{
			FVector2D Result = Perp(V);
			Result.Normalize();
			return Result;
		}

		/**
		 * Perpendicular dot product (2D cross product)
		 */
		FORCEINLINE double PerpDot(const FVector2D& A, const FVector2D& B)
		{
			return A.X * B.Y - A.Y * B.X;
		}

		/**
		 * Test if point is to the left of direction vector (P0 to P1)
		 */
		FORCEINLINE bool IsLeft(const FVector2D& P0, const FVector2D& P1, const FVector2D& Point)
		{
			return (P1.X - P0.X) * (Point.Y - P0.Y) - (P1.Y - P0.Y) * (Point.X - P0.X) > 0.0;
		}

		/**
		 * Test if point is to the left of or on the direction vector
		 */
		FORCEINLINE bool IsLeftOrEqual(const FVector2D& P0, const FVector2D& P1, const FVector2D& Point)
		{
			return (P1.X - P0.X) * (Point.Y - P0.Y) - (P1.Y - P0.Y) * (Point.X - P0.X) >= 0.0;
		}

		/**
		 * Closest point on line segment to a given point
		 */
		FORCEINLINE FVector2D LineSegmentClosestPoint(const FVector2D& P0, const FVector2D& P1, const FVector2D& Point)
		{
			const FVector2D V = P1 - P0;
			const FVector2D W = Point - P0;
			const double C1 = FVector2D::DotProduct(W, V);

			if (C1 < FuzzyEpsilon) { return P0; }

			const double C2 = FVector2D::DotProduct(V, V);
			if (C2 < C1 + FuzzyEpsilon) { return P1; }

			const double B = C1 / C2;
			return P0 + V * B;
		}

		/**
		 * Test if point is within arc sweep region
		 */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		bool PointWithinArcSweep(
			const FVector2D& Center, const FVector2D& ArcStart, const FVector2D& ArcEnd,
			bool bIsClockwise, const FVector2D& Point, double Epsilon = FuzzyEpsilon);

		//~ Arc geometry

		/**
		 * Result of computing arc radius and center
		 */
		struct PCGEXELEMENTSCAVALIERCONTOURS_API FArcGeometry
		{
			double Radius = 0;
			FVector2D Center = FVector2D::ZeroVector;
			bool bValid = false;

			FORCEINLINE FArcGeometry() = default;

			FArcGeometry(double InRadius, const FVector2D& InCenter)
				: Radius(InRadius), Center(InCenter), bValid(true)
			{
			}
		};

		/**
		 * Compute arc radius and center from two vertices defining an arc segment
		 */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		FArcGeometry ComputeArcRadiusAndCenter(const FVertex& V1, const FVertex& V2);

		/**
		 * Calculate arc length for a segment
		 */
		FORCEINLINE double SegmentArcLength(const FVertex& V1, const FVertex& V2)
		{
			if (V1.IsLine()) { return FVector2D::Distance(V1.GetPosition(), V2.GetPosition()); }

			const FArcGeometry Arc = ComputeArcRadiusAndCenter(V1, V2);
			if (!Arc.bValid) { return 0.0; }

			const double StartAngle = Angle(Arc.Center, V1.GetPosition());
			const double EndAngle = Angle(Arc.Center, V2.GetPosition());
			return Arc.Radius * FMath::Abs(DeltaAngle(StartAngle, EndAngle));
		}

		/**
		 * Get midpoint of a segment (line or arc)
		 */
		FORCEINLINE FVector2D SegmentMidpoint(const FVertex& V1, const FVertex& V2)
		{
			if (V1.IsLine()) { return Midpoint(V1.GetPosition(), V2.GetPosition()); }

			const FArcGeometry Arc = ComputeArcRadiusAndCenter(V1, V2);
			if (!Arc.bValid) { return Midpoint(V1.GetPosition(), V2.GetPosition()); }

			const double Angle1 = Angle(Arc.Center, V1.GetPosition());
			const double Angle2 = Angle(Arc.Center, V2.GetPosition());
			const double AngleOffset = DeltaAngleSigned(Angle1, Angle2, V1.Bulge < 0.0) / 2.0;
			const double MidAngle = Angle1 + AngleOffset;

			return PointOnCircle(Arc.Radius, Arc.Center, MidAngle);
		}

		/**
		 * Find closest point on segment (line or arc) to a given point
		 */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		FVector2D SegmentClosestPoint(const FVertex& V1, const FVertex& V2, const FVector2D& Point, double Epsilon = FuzzyEpsilon);

		//~ Line intersections

		/** Line-line intersection result */
		enum class ELineLineIntersectType
		{
			None,
			True,
			False,
			Overlapping
		};

		struct FLineLineIntersect
		{
			ELineLineIntersectType Type = ELineLineIntersectType::None;
			double T1 = 0.0;
			double T2 = 0.0;
			FVector2D Point = FVector2D::ZeroVector;
		};

		/**
		 * Compute line-line intersection
		 */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		FLineLineIntersect LineLineIntersection(
			const FVector2D& P0, const FVector2D& P1,
			const FVector2D& U0, const FVector2D& U1,
			double Epsilon = FuzzyEpsilon);


		//~ Circle intersections

		/** Circle-circle intersection result */
		struct FCircleCircleIntersect
		{
			int32 Count = 0;
			FVector2D Point1 = FVector2D::ZeroVector;
			FVector2D Point2 = FVector2D::ZeroVector;
		};

		/**
		 * Compute circle-circle intersection points
		 */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		FCircleCircleIntersect CircleCircleIntersection(
			const FVector2D& C1, double R1,
			const FVector2D& C2, double R2,
			double Epsilon = FuzzyEpsilon);

		/** Line-circle intersection result */
		struct FLineCircleIntersect
		{
			int32 Count = 0;
			double T1 = 0.0;
			double T2 = 0.0;
			FVector2D Point1 = FVector2D::ZeroVector;
			FVector2D Point2 = FVector2D::ZeroVector;
		};

		/**
		 * Compute line-circle intersection points
		 */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		FLineCircleIntersect LineCircleIntersection(
			const FVector2D& P0, const FVector2D& P1,
			const FVector2D& Center, double Radius,
			double Epsilon = FuzzyEpsilon);

		/** Check if a point lies within an arc's angular sweep */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		FORCEINLINE bool PointOnArcSweep(
			const FVector2D& Center, const FVector2D& ArcStart, const FVector2D& ArcEnd,
			bool bIsCW, const FVector2D& Point, double Eps)
		{
			return PointWithinArcSweep(Center, ArcStart, ArcEnd, bIsCW, Point, Eps);
		}

		/** Substitute endpoint if intersection is very close to it */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		FORCEINLINE FVector2D SubstituteEndpoint(
			const FVector2D& IntersectPt,
			const FVector2D& P1, const FVector2D& P2,
			const FVector2D& Q1, const FVector2D& Q2,
			double Eps)
		{
			// Check against all four endpoints
			if (IntersectPt.Equals(P1, Eps))
			{
				return P1;
			}
			if (IntersectPt.Equals(P2, Eps))
			{
				return P2;
			}
			if (IntersectPt.Equals(Q1, Eps))
			{
				return Q1;
			}
			if (IntersectPt.Equals(Q2, Eps))
			{
				return Q2;
			}
			return IntersectPt;
		}
	}
}
