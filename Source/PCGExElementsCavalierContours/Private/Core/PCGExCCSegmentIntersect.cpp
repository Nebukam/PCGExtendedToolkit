// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#include "Core/PCGExCCSegmentIntersect.h"
#include "Core/PCGExCCMath.h"

namespace PCGExCavalier
{
	// Line-Line Intersection

	FPlineSegIntersect LineLineIntersect(
		const FVector2D& P1, const FVector2D& P2,
		const FVector2D& Q1, const FVector2D& Q2,
		double PosEqualEps)
	{
		const Math::FLineLineIntersect Intr = Math::LineLineIntersection(P1, P2, Q1, Q2, PosEqualEps);

		switch (Intr.Type)
		{
		case Math::ELineLineIntersectType::True:
			{
				// Check if intersection is within both segments
				const FVector2D D1 = P2 - P1;
				const FVector2D D2 = Q2 - Q1;

				const double T1 = D1.SizeSquared() > PosEqualEps * PosEqualEps
					                  ? FVector2D::DotProduct(Intr.Point - P1, D1) / D1.SizeSquared()
					                  : 0.5;
				const double T2 = D2.SizeSquared() > PosEqualEps * PosEqualEps
					                  ? FVector2D::DotProduct(Intr.Point - Q1, D2) / D2.SizeSquared()
					                  : 0.5;

				if (T1 >= -PosEqualEps && T1 <= 1.0 + PosEqualEps &&
					T2 >= -PosEqualEps && T2 <= 1.0 + PosEqualEps)
				{
					FVector2D FinalPt = Math::SubstituteEndpoint(Intr.Point, P1, P2, Q1, Q2, PosEqualEps);

					// Check if it's a tangent (endpoint) intersection
					bool bIsTangent = FinalPt.Equals(P1, PosEqualEps) || FinalPt.Equals(P2, PosEqualEps) ||
						FinalPt.Equals(Q1, PosEqualEps) || FinalPt.Equals(Q2, PosEqualEps);

					return FPlineSegIntersect(
						bIsTangent ? EPlineSegIntersectType::TangentIntersect : EPlineSegIntersectType::OneIntersect,
						FinalPt);
				}
			}
			break;

		case Math::ELineLineIntersectType::Overlapping:
			{
				// Find overlap region
				const FVector2D D = P2 - P1;
				const double LenSq = D.SizeSquared();

				if (LenSq < PosEqualEps * PosEqualEps)
				{
					// Degenerate segment
					return FPlineSegIntersect();
				}

				// Project Q1, Q2 onto P1-P2 line
				double T_Q1 = FVector2D::DotProduct(Q1 - P1, D) / LenSq;
				double T_Q2 = FVector2D::DotProduct(Q2 - P1, D) / LenSq;

				if (T_Q1 > T_Q2)
				{
					Swap(T_Q1, T_Q2);
				}

				const double OverlapStart = FMath::Max(0.0, T_Q1);
				const double OverlapEnd = FMath::Min(1.0, T_Q2);

				if (OverlapEnd > OverlapStart + PosEqualEps)
				{
					FVector2D Pt1 = P1 + D * OverlapStart;
					FVector2D Pt2 = P1 + D * OverlapEnd;

					Pt1 = Math::SubstituteEndpoint(Pt1, P1, P2, Q1, Q2, PosEqualEps);
					Pt2 = Math::SubstituteEndpoint(Pt2, P1, P2, Q1, Q2, PosEqualEps);

					return FPlineSegIntersect(EPlineSegIntersectType::OverlappingLines, Pt1, Pt2);
				}
				if (FMath::Abs(OverlapEnd - OverlapStart) < PosEqualEps)
				{
					// Single point overlap (tangent)
					FVector2D Pt = P1 + D * OverlapStart;
					Pt = Math::SubstituteEndpoint(Pt, P1, P2, Q1, Q2, PosEqualEps);
					return FPlineSegIntersect(EPlineSegIntersectType::TangentIntersect, Pt);
				}
			}
			break;

		default:
			break;
		}

		return FPlineSegIntersect();
	}


	// Line-Arc Intersection


	FPlineSegIntersect LineArcIntersect(
		const FVector2D& LineStart, const FVector2D& LineEnd,
		const FVertex& ArcStart, const FVertex& ArcEnd,
		double PosEqualEps)
	{
		const Math::FArcGeometry Arc = Math::ComputeArcRadiusAndCenter(ArcStart, ArcEnd);
		if (!Arc.bValid)
		{
			// Degenerate arc - treat as line
			return LineLineIntersect(LineStart, LineEnd, ArcStart.GetPosition(), ArcEnd.GetPosition(), PosEqualEps);
		}

		const Math::FLineCircleIntersect CircleIntr = Math::LineCircleIntersection(
			LineStart, LineEnd, Arc.Center, Arc.Radius, PosEqualEps);

		if (CircleIntr.Count == 0)
		{
			return FPlineSegIntersect();
		}

		const bool bArcIsCW = ArcStart.Bulge < 0.0;
		TArray<FVector2D> ValidPoints;

		auto CheckPoint = [&](const FVector2D& Pt, double T) -> bool
		{
			// Check if on line segment
			if (T < -PosEqualEps || T > 1.0 + PosEqualEps)
			{
				return false;
			}

			// Check if on arc sweep
			return Math::PointOnArcSweep(Arc.Center, ArcStart.GetPosition(), ArcEnd.GetPosition(), bArcIsCW, Pt, PosEqualEps);
		};

		if (CircleIntr.Count >= 1 && CheckPoint(CircleIntr.Point1, CircleIntr.T1))
		{
			FVector2D Pt = Math::SubstituteEndpoint(CircleIntr.Point1, LineStart, LineEnd, ArcStart.GetPosition(), ArcEnd.GetPosition(), PosEqualEps);
			ValidPoints.Add(Pt);
		}

		if (CircleIntr.Count >= 2 && CheckPoint(CircleIntr.Point2, CircleIntr.T2))
		{
			FVector2D Pt = Math::SubstituteEndpoint(CircleIntr.Point2, LineStart, LineEnd, ArcStart.GetPosition(), ArcEnd.GetPosition(), PosEqualEps);

			// Avoid duplicates
			if (ValidPoints.IsEmpty() || !ValidPoints[0].Equals(Pt, PosEqualEps))
			{
				ValidPoints.Add(Pt);
			}
		}

		if (ValidPoints.Num() == 0)
		{
			return FPlineSegIntersect();
		}
		if (ValidPoints.Num() == 1)
		{
			// Check if tangent
			bool bIsTangent = ValidPoints[0].Equals(LineStart, PosEqualEps) ||
				ValidPoints[0].Equals(LineEnd, PosEqualEps) ||
				ValidPoints[0].Equals(ArcStart.GetPosition(), PosEqualEps) ||
				ValidPoints[0].Equals(ArcEnd.GetPosition(), PosEqualEps);

			return FPlineSegIntersect(
				bIsTangent ? EPlineSegIntersectType::TangentIntersect : EPlineSegIntersectType::OneIntersect,
				ValidPoints[0]);
		}
		// Order by distance from arc start
		if (Math::DistanceSquared(ArcStart.GetPosition(), ValidPoints[0]) >
			Math::DistanceSquared(ArcStart.GetPosition(), ValidPoints[1]))
		{
			Swap(ValidPoints[0], ValidPoints[1]);
		}

		return FPlineSegIntersect(EPlineSegIntersectType::TwoIntersects, ValidPoints[0], ValidPoints[1]);
	}


	// Arc-Arc Intersection


	FPlineSegIntersect ArcArcIntersect(
		const FVertex& Arc1Start, const FVertex& Arc1End,
		const FVertex& Arc2Start, const FVertex& Arc2End,
		double PosEqualEps)
	{
		const Math::FArcGeometry Arc1 = Math::ComputeArcRadiusAndCenter(Arc1Start, Arc1End);
		const Math::FArcGeometry Arc2 = Math::ComputeArcRadiusAndCenter(Arc2Start, Arc2End);

		if (!Arc1.bValid || !Arc2.bValid)
		{
			// Degenerate - fall back to line intersection
			if (!Arc1.bValid && !Arc2.bValid)
			{
				return LineLineIntersect(Arc1Start.GetPosition(), Arc1End.GetPosition(),
				                         Arc2Start.GetPosition(), Arc2End.GetPosition(), PosEqualEps);
			}
			if (!Arc1.bValid)
			{
				return LineArcIntersect(Arc1Start.GetPosition(), Arc1End.GetPosition(),
				                        Arc2Start, Arc2End, PosEqualEps);
			}
			return LineArcIntersect(Arc2Start.GetPosition(), Arc2End.GetPosition(),
			                        Arc1Start, Arc1End, PosEqualEps);
		}

		// Check for concentric arcs
		if (Arc1.Center.Equals(Arc2.Center, PosEqualEps) &&
			FMath::Abs(Arc1.Radius - Arc2.Radius) < PosEqualEps)
		{
			// Same circle - check for arc overlap
			const bool bArc1IsCW = Arc1Start.Bulge < 0.0;
			const bool bArc2IsCW = Arc2Start.Bulge < 0.0;

			// Check if arc endpoints lie on each other's sweeps
			TArray<FVector2D> OverlapPoints;

			if (Math::PointOnArcSweep(Arc1.Center, Arc1Start.GetPosition(), Arc1End.GetPosition(), bArc1IsCW, Arc2Start.GetPosition(), PosEqualEps))
			{
				OverlapPoints.AddUnique(Arc2Start.GetPosition());
			}
			if (Math::PointOnArcSweep(Arc1.Center, Arc1Start.GetPosition(), Arc1End.GetPosition(), bArc1IsCW, Arc2End.GetPosition(), PosEqualEps))
			{
				OverlapPoints.AddUnique(Arc2End.GetPosition());
			}
			if (Math::PointOnArcSweep(Arc2.Center, Arc2Start.GetPosition(), Arc2End.GetPosition(), bArc2IsCW, Arc1Start.GetPosition(), PosEqualEps))
			{
				OverlapPoints.AddUnique(Arc1Start.GetPosition());
			}
			if (Math::PointOnArcSweep(Arc2.Center, Arc2Start.GetPosition(), Arc2End.GetPosition(), bArc2IsCW, Arc1End.GetPosition(), PosEqualEps))
			{
				OverlapPoints.AddUnique(Arc1End.GetPosition());
			}

			if (OverlapPoints.Num() >= 2)
			{
				return FPlineSegIntersect(EPlineSegIntersectType::OverlappingArcs,
				                          OverlapPoints[0], OverlapPoints[1]);
			}
			if (OverlapPoints.Num() == 1)
			{
				return FPlineSegIntersect(EPlineSegIntersectType::TangentIntersect, OverlapPoints[0]);
			}

			return FPlineSegIntersect();
		}

		// Different circles - compute circle-circle intersection
		const Math::FCircleCircleIntersect CircleIntr = Math::CircleCircleIntersection(
			Arc1.Center, Arc1.Radius, Arc2.Center, Arc2.Radius, PosEqualEps);

		if (CircleIntr.Count == 0)
		{
			return FPlineSegIntersect();
		}

		const bool bArc1IsCW = Arc1Start.Bulge < 0.0;
		const bool bArc2IsCW = Arc2Start.Bulge < 0.0;

		TArray<FVector2D> ValidPoints;

		auto CheckPoint = [&](const FVector2D& Pt) -> bool
		{
			bool bOnArc1 = Math::PointOnArcSweep(Arc1.Center, Arc1Start.GetPosition(), Arc1End.GetPosition(), bArc1IsCW, Pt, PosEqualEps);
			bool bOnArc2 = Math::PointOnArcSweep(Arc2.Center, Arc2Start.GetPosition(), Arc2End.GetPosition(), bArc2IsCW, Pt, PosEqualEps);
			return bOnArc1 && bOnArc2;
		};

		if (CircleIntr.Count >= 1 && CheckPoint(CircleIntr.Point1))
		{
			FVector2D Pt = Math::SubstituteEndpoint(CircleIntr.Point1, Arc1Start.GetPosition(), Arc1End.GetPosition(), Arc2Start.GetPosition(), Arc2End.GetPosition(), PosEqualEps);
			ValidPoints.Add(Pt);
		}

		if (CircleIntr.Count >= 2 && CheckPoint(CircleIntr.Point2))
		{
			FVector2D Pt = Math::SubstituteEndpoint(CircleIntr.Point2, Arc1Start.GetPosition(), Arc1End.GetPosition(), Arc2Start.GetPosition(), Arc2End.GetPosition(), PosEqualEps);

			if (ValidPoints.IsEmpty() || !ValidPoints[0].Equals(Pt, PosEqualEps))
			{
				ValidPoints.Add(Pt);
			}
		}

		if (ValidPoints.Num() == 0)
		{
			return FPlineSegIntersect();
		}
		if (ValidPoints.Num() == 1)
		{
			bool bIsTangent = ValidPoints[0].Equals(Arc1Start.GetPosition(), PosEqualEps) ||
				ValidPoints[0].Equals(Arc1End.GetPosition(), PosEqualEps) ||
				ValidPoints[0].Equals(Arc2Start.GetPosition(), PosEqualEps) ||
				ValidPoints[0].Equals(Arc2End.GetPosition(), PosEqualEps);

			return FPlineSegIntersect(
				bIsTangent ? EPlineSegIntersectType::TangentIntersect : EPlineSegIntersectType::OneIntersect,
				ValidPoints[0]);
		}
		// Order by distance from arc1 start
		if (Math::DistanceSquared(Arc1Start.GetPosition(), ValidPoints[0]) >
			Math::DistanceSquared(Arc1Start.GetPosition(), ValidPoints[1]))
		{
			Swap(ValidPoints[0], ValidPoints[1]);
		}

		return FPlineSegIntersect(EPlineSegIntersectType::TwoIntersects, ValidPoints[0], ValidPoints[1]);
	}


	// Main Segment Intersection


	FPlineSegIntersect PlineSegmentIntersect(
		const FVertex& V1, const FVertex& V2,
		const FVertex& U1, const FVertex& U2,
		double PosEqualEps)
	{
		const bool bV1IsLine = V1.IsLine(PosEqualEps);
		const bool bU1IsLine = U1.IsLine(PosEqualEps);

		if (bV1IsLine && bU1IsLine)
		{
			return LineLineIntersect(V1.GetPosition(), V2.GetPosition(),
			                         U1.GetPosition(), U2.GetPosition(), PosEqualEps);
		}
		if (bV1IsLine && !bU1IsLine)
		{
			return LineArcIntersect(V1.GetPosition(), V2.GetPosition(), U1, U2, PosEqualEps);
		}
		if (!bV1IsLine && bU1IsLine)
		{
			// Swap order and adjust result
			FPlineSegIntersect Result = LineArcIntersect(U1.GetPosition(), U2.GetPosition(), V1, V2, PosEqualEps);

			// For two intersects, reorder by distance from V1
			if (Result.Type == EPlineSegIntersectType::TwoIntersects)
			{
				if (Math::DistanceSquared(V1.GetPosition(), Result.Point1) >
					Math::DistanceSquared(V1.GetPosition(), Result.Point2))
				{
					Swap(Result.Point1, Result.Point2);
				}
			}

			return Result;
		}
		return ArcArcIntersect(V1, V2, U1, U2, PosEqualEps);
	}
}
