// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#pragma once

#include "CoreMinimal.h"
#include "PCGExCCTypes.h"

namespace PCGExCavalier
{
	/**
	 * Type of intersection between two polyline segments
	 */
	enum class EPlineSegIntersectType : uint8
	{
		/** No intersection */
		NoIntersect,

		/** Tangent intersection (segments just touch) */
		TangentIntersect,

		/** Single intersection point */
		OneIntersect,

		/** Two intersection points (arc-arc or arc-line) */
		TwoIntersects,

		/** Overlapping collinear line segments */
		OverlappingLines,

		/** Overlapping concentric arcs */
		OverlappingArcs
	};

	/**
	 * Result of segment-segment intersection
	 */
	struct PCGEXELEMENTSCAVALIERCONTOURS_API FPlineSegIntersect
	{
		EPlineSegIntersectType Type = EPlineSegIntersectType::NoIntersect;
		FVector2D Point1 = FVector2D::ZeroVector;
		FVector2D Point2 = FVector2D::ZeroVector;

		FPlineSegIntersect() = default;

		FPlineSegIntersect(EPlineSegIntersectType InType)
			: Type(InType)
		{
		}

		FPlineSegIntersect(EPlineSegIntersectType InType, const FVector2D& InPoint1)
			: Type(InType)
			  , Point1(InPoint1)
		{
		}

		FPlineSegIntersect(EPlineSegIntersectType InType, const FVector2D& InPoint1, const FVector2D& InPoint2)
			: Type(InType)
			  , Point1(InPoint1)
			  , Point2(InPoint2)
		{
		}

		bool HasIntersection() const
		{
			return Type != EPlineSegIntersectType::NoIntersect;
		}

		int32 IntersectionCount() const
		{
			switch (Type)
			{
			case EPlineSegIntersectType::NoIntersect:
				return 0;
			case EPlineSegIntersectType::TangentIntersect:
			case EPlineSegIntersectType::OneIntersect:
				return 1;
			case EPlineSegIntersectType::TwoIntersects:
			case EPlineSegIntersectType::OverlappingLines:
			case EPlineSegIntersectType::OverlappingArcs:
				return 2;
			default:
				return 0;
			}
		}
	};

	/**
	 * Compute intersection between two polyline segments.
	 * Handles all combinations: line-line, line-arc, arc-line, arc-arc.
	 * 
	 * @param V1 Start vertex of first segment
	 * @param V2 End vertex of first segment
	 * @param U1 Start vertex of second segment
	 * @param U2 End vertex of second segment
	 * @param PosEqualEps Epsilon for position equality
	 * @return Intersection result
	 */
	PCGEXELEMENTSCAVALIERCONTOURS_API
	FPlineSegIntersect PlineSegmentIntersect(
		const FVertex& V1, const FVertex& V2,
		const FVertex& U1, const FVertex& U2,
		double PosEqualEps);

	/**
	 * Compute intersection between two line segments.
	 */
	PCGEXELEMENTSCAVALIERCONTOURS_API
	FPlineSegIntersect LineLineIntersect(
		const FVector2D& P1, const FVector2D& P2,
		const FVector2D& Q1, const FVector2D& Q2,
		double PosEqualEps);

	/**
	 * Compute intersection between a line segment and an arc segment.
	 */
	PCGEXELEMENTSCAVALIERCONTOURS_API
	FPlineSegIntersect LineArcIntersect(
		const FVector2D& LineStart, const FVector2D& LineEnd,
		const FVertex& ArcStart, const FVertex& ArcEnd,
		double PosEqualEps);

	/**
	 * Compute intersection between two arc segments.
	 */
	PCGEXELEMENTSCAVALIERCONTOURS_API
	FPlineSegIntersect ArcArcIntersect(
		const FVertex& Arc1Start, const FVertex& Arc1End,
		const FVertex& Arc2Start, const FVertex& Arc2End,
		double PosEqualEps);
}
