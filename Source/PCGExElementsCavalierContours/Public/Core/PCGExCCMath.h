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
		constexpr double FuzzyEpsilon = 1e-9;

		/** Two times PI */
		constexpr double TwoPI = 2.0 * PI;

		//~ Angle utilities
	
		/**
		 * Normalize radians to be between 0 and 2*PI
		 */
		inline double NormalizeRadians(double Angle)
		{
			if (Angle >= 0.0 && Angle <= TwoPI)
			{
				return Angle;
			}
			return Angle - FMath::Floor(Angle / TwoPI) * TwoPI;
		}

		/**
		 * Returns the smaller difference between two angles.
		 * Result is negative if normalize_radians(angle2 - angle1) > PI
		 */
		inline double DeltaAngle(double Angle1, double Angle2)
		{
			double Diff = NormalizeRadians(Angle2 - Angle1);
			if (Diff > PI)
			{
				Diff = Diff - TwoPI;
			}
			return Diff;
		}

		/**
		 * Returns the delta angle with a specific sign applied
		 */
		inline double DeltaAngleSigned(double Angle1, double Angle2, bool bNegative)
		{
			const double Diff = DeltaAngle(Angle1, Angle2);
			return bNegative ? -FMath::Abs(Diff) : FMath::Abs(Diff);
		}

		/**
		 * Tests if test_angle is between start_angle and end_angle (counter-clockwise sweep)
		 */
		inline bool AngleIsBetween(double TestAngle, double StartAngle, double EndAngle, double Epsilon = FuzzyEpsilon)
		{
			const double EndSweep = NormalizeRadians(EndAngle - StartAngle);
			const double MidSweep = NormalizeRadians(TestAngle - StartAngle);
			return MidSweep < EndSweep + Epsilon;
		}

		/**
		 * Tests if test_angle is within the sweep_angle starting at start_angle
		 */
		inline bool AngleIsWithinSweep(double TestAngle, double StartAngle, double SweepAngle, double Epsilon = FuzzyEpsilon)
		{
			const double EndAngle = StartAngle + SweepAngle;
			if (SweepAngle < 0.0)
			{
				return AngleIsBetween(TestAngle, EndAngle, StartAngle, Epsilon);
			}
			return AngleIsBetween(TestAngle, StartAngle, EndAngle, Epsilon);
		}

		//~ Bulge/Arc utilities
	
		/**
		 * Convert arc sweep angle to bulge value
		 * bulge = tan(sweep_angle / 4)
		 */
		inline double BulgeFromAngle(double SweepAngle)
		{
			return FMath::Tan(SweepAngle / 4.0);
		}

		/**
		 * Convert bulge value to arc sweep angle
		 * sweep_angle = 4 * atan(bulge)
		 */
		inline double AngleFromBulge(double Bulge)
		{
			return 4.0 * FMath::Atan(Bulge);
		}

		//~ Point/Vector utilities

		/**
		 * Get angle of direction vector from P0 to P1
		 */
		inline double Angle(const FVector2D& P0, const FVector2D& P1)
		{
			return FMath::Atan2(P1.Y - P0.Y, P1.X - P0.X);
		}

		/**
		 * Squared distance between two points
		 */
		inline double DistanceSquared(const FVector2D& P0, const FVector2D& P1)
		{
			const FVector2D D = P0 - P1;
			return FVector2D::DotProduct(D, D);
		}

		/**
		 * Get midpoint between two points
		 */
		inline FVector2D Midpoint(const FVector2D& P0, const FVector2D& P1)
		{
			return FVector2D((P0.X + P1.X) / 2.0, (P0.Y + P1.Y) / 2.0);
		}

		/**
		 * Point on circle at given angle
		 */
		inline FVector2D PointOnCircle(double Radius, const FVector2D& Center, double AngleRadians)
		{
			double S, C;
			FMath::SinCos(&S, &C, AngleRadians);
			return FVector2D(Center.X + Radius * C, Center.Y + Radius * S);
		}

		/**
		 * Point from parametric value on line segment
		 */
		inline FVector2D PointFromParametric(const FVector2D& P0, const FVector2D& P1, double T)
		{
			return P0 + (P1 - P0) * T;
		}

		/**
		 * Perpendicular vector (rotated 90 degrees CCW)
		 */
		inline FVector2D Perp(const FVector2D& V)
		{
			return FVector2D(-V.Y, V.X);
		}

		/**
		 * Unit perpendicular vector
		 */
		inline FVector2D UnitPerp(const FVector2D& V)
		{
			FVector2D Result = Perp(V);
			Result.Normalize();
			return Result;
		}

		/**
		 * Perpendicular dot product (2D cross product)
		 */
		inline double PerpDot(const FVector2D& A, const FVector2D& B)
		{
			return A.X * B.Y - A.Y * B.X;
		}

		/**
		 * Test if point is to the left of direction vector (P0 to P1)
		 */
		inline bool IsLeft(const FVector2D& P0, const FVector2D& P1, const FVector2D& Point)
		{
			return (P1.X - P0.X) * (Point.Y - P0.Y) - (P1.Y - P0.Y) * (Point.X - P0.X) > 0.0;
		}

		/**
		 * Test if point is to the left of or on the direction vector
		 */
		inline bool IsLeftOrEqual(const FVector2D& P0, const FVector2D& P1, const FVector2D& Point)
		{
			return (P1.X - P0.X) * (Point.Y - P0.Y) - (P1.Y - P0.Y) * (Point.X - P0.X) >= 0.0;
		}

		/**
		 * Closest point on line segment to a given point
		 */
		inline FVector2D LineSegmentClosestPoint(const FVector2D& P0, const FVector2D& P1, const FVector2D& Point)
		{
			const FVector2D V = P1 - P0;
			const FVector2D W = Point - P0;
			const double C1 = FVector2D::DotProduct(W, V);
		
			if (C1 < FuzzyEpsilon)
			{
				return P0;
			}
		
			const double C2 = FVector2D::DotProduct(V, V);
			if (C2 < C1 + FuzzyEpsilon)
			{
				return P1;
			}
		
			const double B = C1 / C2;
			return P0 + V * B;
		}

		/**
		 * Test if point is within arc sweep region
		 */
		inline bool PointWithinArcSweep(
			const FVector2D& Center,
			const FVector2D& ArcStart,
			const FVector2D& ArcEnd,
			bool bIsClockwise,
			const FVector2D& Point,
			double Epsilon = FuzzyEpsilon)
		{
			auto IsLeftOrCoincident = [Epsilon](const FVector2D& P0, const FVector2D& P1, const FVector2D& Pt) -> bool
			{
				return (P1.X - P0.X) * (Pt.Y - P0.Y) - (P1.Y - P0.Y) * (Pt.X - P0.X) > -Epsilon;
			};
		
			auto IsRightOrCoincident = [Epsilon](const FVector2D& P0, const FVector2D& P1, const FVector2D& Pt) -> bool
			{
				return (P1.X - P0.X) * (Pt.Y - P0.Y) - (P1.Y - P0.Y) * (Pt.X - P0.X) < Epsilon;
			};

			if (bIsClockwise)
			{
				return IsRightOrCoincident(Center, ArcStart, Point) && IsLeftOrCoincident(Center, ArcEnd, Point);
			}
		
			return IsLeftOrCoincident(Center, ArcStart, Point) && IsRightOrCoincident(Center, ArcEnd, Point);
		}

		//~ Arc geometry

		/**
		 * Result of computing arc radius and center
		 */
		struct FArcGeometry
		{
			double Radius;
			FVector2D Center;
			bool bValid;

			FArcGeometry() : Radius(0.0), Center(FVector2D::ZeroVector), bValid(false) {}
			FArcGeometry(double InRadius, const FVector2D& InCenter) : Radius(InRadius), Center(InCenter), bValid(true) {}
		};

		/**
		 * Compute arc radius and center from two vertices defining an arc segment
		 */
		inline FArcGeometry ComputeArcRadiusAndCenter(const FVertex& V1, const FVertex& V2)
		{
			if (V1.IsLine())
			{
				return FArcGeometry();
			}

			const FVector2D Pos1 = V1.GetPosition();
			const FVector2D Pos2 = V2.GetPosition();
		
			if (Pos1.Equals(Pos2, FuzzyEpsilon))
			{
				return FArcGeometry();
			}

			const double AbsBulge = FMath::Abs(V1.Bulge);
			const FVector2D ChordVec = Pos2 - Pos1;
			const double ChordLen = ChordVec.Size();
		
			// Compute radius: r = chord * (bulge^2 + 1) / (4 * bulge)
			const double Radius = ChordLen * (AbsBulge * AbsBulge + 1.0) / (4.0 * AbsBulge);
		
			// Compute center offset from chord midpoint
			const double S = AbsBulge * ChordLen / 2.0;
			const double M = Radius - S;
			double OffsX = -M * ChordVec.Y / ChordLen;
			double OffsY = M * ChordVec.X / ChordLen;
		
			if (V1.Bulge < 0.0)
			{
				OffsX = -OffsX;
				OffsY = -OffsY;
			}
		
			const FVector2D Center(
				Pos1.X + ChordVec.X / 2.0 + OffsX,
				Pos1.Y + ChordVec.Y / 2.0 + OffsY
			);

			return FArcGeometry(Radius, Center);
		}

		/**
		 * Calculate arc length for a segment
		 */
		inline double SegmentArcLength(const FVertex& V1, const FVertex& V2)
		{
			if (V1.IsLine())
			{
				return FVector2D::Distance(V1.GetPosition(), V2.GetPosition());
			}
		
			const FArcGeometry Arc = ComputeArcRadiusAndCenter(V1, V2);
			if (!Arc.bValid)
			{
				return 0.0;
			}
		
			const double StartAngle = Angle(Arc.Center, V1.GetPosition());
			const double EndAngle = Angle(Arc.Center, V2.GetPosition());
			return Arc.Radius * FMath::Abs(DeltaAngle(StartAngle, EndAngle));
		}

		/**
		 * Get midpoint of a segment (line or arc)
		 */
		inline FVector2D SegmentMidpoint(const FVertex& V1, const FVertex& V2)
		{
			if (V1.IsLine())
			{
				return Midpoint(V1.GetPosition(), V2.GetPosition());
			}
		
			const FArcGeometry Arc = ComputeArcRadiusAndCenter(V1, V2);
			if (!Arc.bValid)
			{
				return Midpoint(V1.GetPosition(), V2.GetPosition());
			}
		
			const double Angle1 = Angle(Arc.Center, V1.GetPosition());
			const double Angle2 = Angle(Arc.Center, V2.GetPosition());
			const double AngleOffset = DeltaAngleSigned(Angle1, Angle2, V1.Bulge < 0.0) / 2.0;
			const double MidAngle = Angle1 + AngleOffset;
		
			return PointOnCircle(Arc.Radius, Arc.Center, MidAngle);
		}

		/**
		 * Find closest point on segment (line or arc) to a given point
		 */
		inline FVector2D SegmentClosestPoint(const FVertex& V1, const FVertex& V2, const FVector2D& Point, double Epsilon = FuzzyEpsilon)
		{
			const FVector2D Pos1 = V1.GetPosition();
			const FVector2D Pos2 = V2.GetPosition();
		
			if (V1.IsLine())
			{
				return LineSegmentClosestPoint(Pos1, Pos2, Point);
			}
		
			const FArcGeometry Arc = ComputeArcRadiusAndCenter(V1, V2);
			if (!Arc.bValid)
			{
				return LineSegmentClosestPoint(Pos1, Pos2, Point);
			}
		
			// Check if point is at center
			if (Point.Equals(Arc.Center, Epsilon))
			{
				return Pos1;
			}
		
			// Check if point projects onto arc
			if (PointWithinArcSweep(Arc.Center, Pos1, Pos2, V1.Bulge < 0.0, Point, Epsilon))
			{
				FVector2D ToPoint = Point - Arc.Center;
				ToPoint.Normalize();
				return Arc.Center + ToPoint * Arc.Radius;
			}
		
			// Closest is one of the endpoints
			const double Dist1 = DistanceSquared(Pos1, Point);
			const double Dist2 = DistanceSquared(Pos2, Point);
			return Dist1 < Dist2 ? Pos1 : Pos2;
		}

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
		inline FLineLineIntersect LineLineIntersection(
			const FVector2D& P0, const FVector2D& P1,
			const FVector2D& U0, const FVector2D& U1,
			double Epsilon = FuzzyEpsilon)
		{
			FLineLineIntersect Result;
		
			const FVector2D D1 = P1 - P0;
			const FVector2D D2 = U1 - U0;
			const double Cross = PerpDot(D1, D2);
		
			const FVector2D D0 = U0 - P0;
		
			if (FMath::Abs(Cross) < Epsilon)
			{
				// Lines are parallel
				const double CrossD0 = PerpDot(D0, D1);
				if (FMath::Abs(CrossD0) < Epsilon)
				{
					Result.Type = ELineLineIntersectType::Overlapping;
				}
				else
				{
					Result.Type = ELineLineIntersectType::None;
				}
				return Result;
			}
		
			const double T1 = PerpDot(D0, D2) / Cross;
			const double T2 = PerpDot(D0, D1) / Cross;
		
			Result.T1 = T1;
			Result.T2 = T2;
			Result.Point = P0 + D1 * T1;
		
			// Check if intersection is within both segments
			if (T1 >= -Epsilon && T1 <= 1.0 + Epsilon && T2 >= -Epsilon && T2 <= 1.0 + Epsilon)
			{
				Result.Type = ELineLineIntersectType::True;
			}
			else
			{
				Result.Type = ELineLineIntersectType::False;
			}
		
			return Result;
		}

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
		inline FCircleCircleIntersect CircleCircleIntersection(
			const FVector2D& C1, double R1,
			const FVector2D& C2, double R2,
			double Epsilon = FuzzyEpsilon)
		{
			FCircleCircleIntersect Result;
		
			const FVector2D D = C2 - C1;
			const double DistSq = FVector2D::DotProduct(D, D);
			const double Dist = FMath::Sqrt(DistSq);
		
			if (Dist < Epsilon)
			{
				// Circles are concentric
				return Result;
			}
		
			const double SumR = R1 + R2;
			const double DiffR = FMath::Abs(R1 - R2);
		
			if (Dist > SumR + Epsilon || Dist < DiffR - Epsilon)
			{
				// No intersection
				return Result;
			}
		
			if (FMath::Abs(Dist - SumR) < Epsilon || FMath::Abs(Dist - DiffR) < Epsilon)
			{
				// Single intersection point
				Result.Count = 1;
				Result.Point1 = C1 + D * (R1 / Dist);
				return Result;
			}
		
			// Two intersection points
			const double A = (R1 * R1 - R2 * R2 + DistSq) / (2.0 * Dist);
			const double HSq = R1 * R1 - A * A;
		
			if (HSq < 0.0)
			{
				return Result;
			}
		
			const double H = FMath::Sqrt(HSq);
			const FVector2D P = C1 + D * (A / Dist);
			const FVector2D Offset(-D.Y * H / Dist, D.X * H / Dist);
		
			Result.Count = 2;
			Result.Point1 = P + Offset;
			Result.Point2 = P - Offset;
		
			return Result;
		}

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
		inline FLineCircleIntersect LineCircleIntersection(
			const FVector2D& P0, const FVector2D& P1,
			const FVector2D& Center, double Radius,
			double Epsilon = FuzzyEpsilon)
		{
			FLineCircleIntersect Result;
		
			const FVector2D D = P1 - P0;
			const FVector2D F = P0 - Center;
		
			const double A = FVector2D::DotProduct(D, D);
			const double B = 2.0 * FVector2D::DotProduct(F, D);
			const double C = FVector2D::DotProduct(F, F) - Radius * Radius;
		
			const double Discriminant = B * B - 4.0 * A * C;
		
			if (Discriminant < -Epsilon)
			{
				return Result;
			}
		
			if (Discriminant < Epsilon)
			{
				// Tangent (single intersection)
				Result.Count = 1;
				Result.T1 = -B / (2.0 * A);
				Result.Point1 = P0 + D * Result.T1;
				return Result;
			}
		
			// Two intersections
			const double SqrtDisc = FMath::Sqrt(Discriminant);
			Result.Count = 2;
		
			if (B < 0.0)
			{
				Result.T1 = (-B + SqrtDisc) / (2.0 * A);
			}
			else
			{
				Result.T1 = (-B - SqrtDisc) / (2.0 * A);
			}
			Result.T2 = C / (A * Result.T1);
		
			Result.Point1 = P0 + D * Result.T1;
			Result.Point2 = P0 + D * Result.T2;
		
			return Result;
		}
	}
}