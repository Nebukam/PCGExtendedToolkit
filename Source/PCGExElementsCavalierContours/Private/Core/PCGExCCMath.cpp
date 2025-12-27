// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#include "Core/PCGExCCMath.h"

namespace PCGExCavalier::Math
{
	bool PointWithinArcSweep(const FVector2D& Center, const FVector2D& ArcStart, const FVector2D& ArcEnd, bool bIsClockwise, const FVector2D& Point, double Epsilon)
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

	FArcGeometry ComputeArcRadiusAndCenter(const FVertex& V1, const FVertex& V2)
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

	FVector2D SegmentClosestPoint(const FVertex& V1, const FVertex& V2, const FVector2D& Point, double Epsilon)
	{
		const FVector2D Pos1 = V1.GetPosition();
		const FVector2D Pos2 = V2.GetPosition();

		if (V1.IsLine()) { return LineSegmentClosestPoint(Pos1, Pos2, Point); }

		const FArcGeometry Arc = ComputeArcRadiusAndCenter(V1, V2);
		if (!Arc.bValid) { return LineSegmentClosestPoint(Pos1, Pos2, Point); }

		// Check if point is at center
		if (Point.Equals(Arc.Center, Epsilon)) { return Pos1; }

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

	FLineLineIntersect LineLineIntersection(const FVector2D& P0, const FVector2D& P1, const FVector2D& U0, const FVector2D& U1, double Epsilon)
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

	FCircleCircleIntersect CircleCircleIntersection(const FVector2D& C1, double R1, const FVector2D& C2, double R2, double Epsilon)
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

	FLineCircleIntersect LineCircleIntersection(const FVector2D& P0, const FVector2D& P1, const FVector2D& Center, double Radius, double Epsilon)
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
