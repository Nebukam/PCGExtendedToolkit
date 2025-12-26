// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#include "Core/PCGExCCOffset.h"
#include "Core/PCGExCCPolyline.h"

//~ FContourUtils Implementation

namespace PCGExCavalier
{
	FContourResult3D FContourUtils::ConvertTo3D(
		const FPolyline& Polyline2D,
		const TArray<FInputPoint>& SourcePoints,
		bool bClosed)
	{
		FContourResult3D Result;
		Result.bIsClosed = bClosed;

		const int32 NumVerts = Polyline2D.VertexCount();
		if (NumVerts == 0 || SourcePoints.Num() == 0)
		{
			return Result;
		}

		Result.Positions.Reserve(NumVerts);
		Result.Transforms.Reserve(NumVerts);

		// Create a lookup for finding the nearest source points for Z interpolation
		// This is a simplified implementation - ideally we would track correspondence
		// through the tessellation process

		for (int32 i = 0; i < NumVerts; ++i)
		{
			const FVertex& V = Polyline2D.GetVertex(i);
			const FVector2D Pos2D = V.GetPosition();

			// Find the two nearest source points for interpolation
			double MinDist1 = TNumericLimits<double>::Max();
			double MinDist2 = TNumericLimits<double>::Max();
			int32 NearestIdx1 = 0;
			int32 NearestIdx2 = 0;

			for (int32 j = 0; j < SourcePoints.Num(); ++j)
			{
				const FVector2D SourcePos2D(SourcePoints[j].Position.X, SourcePoints[j].Position.Y);
				const double Dist = FVector2D::Distance(Pos2D, SourcePos2D);

				if (Dist < MinDist1)
				{
					MinDist2 = MinDist1;
					NearestIdx2 = NearestIdx1;
					MinDist1 = Dist;
					NearestIdx1 = j;
				}
				else if (Dist < MinDist2)
				{
					MinDist2 = Dist;
					NearestIdx2 = j;
				}
			}

			// Interpolate Z based on distance to nearest points
			double Z;
			if (MinDist1 < Math::FuzzyEpsilon)
			{
				// Exactly at a source point
				Z = SourcePoints[NearestIdx1].Position.Z;
			}
			else if (MinDist2 < Math::FuzzyEpsilon || SourcePoints.Num() == 1)
			{
				// Only one valid source point
				Z = SourcePoints[NearestIdx1].Position.Z;
			}
			else
			{
				// Linear interpolation based on inverse distance weighting
				const double W1 = 1.0 / MinDist1;
				const double W2 = 1.0 / MinDist2;
				const double TotalW = W1 + W2;
				Z = (SourcePoints[NearestIdx1].Position.Z * W1 + SourcePoints[NearestIdx2].Position.Z * W2) / TotalW;
			}

			const FVector Pos3D(Pos2D.X, Pos2D.Y, Z);
			Result.Positions.Add(Pos3D);
			Result.Transforms.Add(FTransform(Pos3D));
		}

		return Result;
	}

	TArray<FTransform> FContourUtils::ToTransforms(
		const FPolyline& Polyline2D,
		const TArray<double>& SourceZValues,
		bool bClosed)
	{
		TArray<FTransform> Result;

		const int32 NumVerts = Polyline2D.VertexCount();
		if (NumVerts == 0)
		{
			return Result;
		}

		Result.Reserve(NumVerts);

		// Simple linear interpolation along the polyline
		const double TotalLength = Polyline2D.PathLength();
		double AccumulatedLength = 0.0;
		const int32 NumSourceZ = SourceZValues.Num();

		for (int32 i = 0; i < NumVerts; ++i)
		{
			const FVertex& V = Polyline2D.GetVertex(i);
			const FVector2D Pos2D = V.GetPosition();

			// Compute parametric position along the polyline
			double T = 0.0;
			if (TotalLength > Math::FuzzyEpsilon && i > 0)
			{
				// Add distance from previous vertex
				const FVertex& PrevV = Polyline2D.GetVertex(i - 1);
				AccumulatedLength += Math::SegmentArcLength(PrevV, V);
				T = FMath::Clamp(AccumulatedLength / TotalLength, 0.0, 1.0);
			}

			// Interpolate Z from source values
			double Z;
			if (NumSourceZ == 0)
			{
				Z = 0.0;
			}
			else if (NumSourceZ == 1)
			{
				Z = SourceZValues[0];
			}
			else
			{
				// Linear interpolation between source Z values
				const double SourceT = T * (NumSourceZ - 1);
				const int32 LowIdx = FMath::FloorToInt(SourceT);
				const int32 HighIdx = FMath::Min(LowIdx + 1, NumSourceZ - 1);
				const double Frac = SourceT - LowIdx;
				Z = FMath::Lerp(SourceZValues[LowIdx], SourceZValues[HighIdx], Frac);
			}

			Result.Add(FTransform(FVector(Pos2D.X, Pos2D.Y, Z)));
		}

		return Result;
	}

	FPolyline FContourUtils::ProcessCorners(
		const TArray<FInputPoint>& Points,
		bool bClosed)
	{
		FPolyline Result(bClosed);

		if (Points.Num() < 2)
		{
			// Not enough points for a polyline
			for (const FInputPoint& P : Points)
			{
				Result.AddVertex(P.Position.X, P.Position.Y, 0.0);
			}
			return Result;
		}

		Result.Reserve(Points.Num() * 2); // Reserve extra for potential corner arcs

		for (int32 i = 0; i < Points.Num(); ++i)
		{
			const FInputPoint& Current = Points[i];

			if (!Current.bIsCorner || Current.CornerRadius <= Math::FuzzyEpsilon)
			{
				// Regular point or no radius - just add as line vertex
				Result.AddVertex(Current.Position.X, Current.Position.Y, 0.0);
				continue;
			}

			// Get previous and next points for corner calculation
			const int32 PrevIdx = (i == 0) ? (bClosed ? Points.Num() - 1 : i) : i - 1;
			const int32 NextIdx = (i == Points.Num() - 1) ? (bClosed ? 0 : i) : i + 1;

			// Skip if at endpoint of open polyline
			if (!bClosed && (i == 0 || i == Points.Num() - 1))
			{
				Result.AddVertex(Current.Position.X, Current.Position.Y, 0.0);
				continue;
			}

			const FVector2D PrevPos(Points[PrevIdx].Position.X, Points[PrevIdx].Position.Y);
			const FVector2D CurrPos(Current.Position.X, Current.Position.Y);
			const FVector2D NextPos(Points[NextIdx].Position.X, Points[NextIdx].Position.Y);

			// Vectors to previous and next points
			FVector2D ToPrev = PrevPos - CurrPos;
			FVector2D ToNext = NextPos - CurrPos;
			const double LenPrev = ToPrev.Size();
			const double LenNext = ToNext.Size();

			if (LenPrev < Math::FuzzyEpsilon || LenNext < Math::FuzzyEpsilon)
			{
				// Degenerate case
				Result.AddVertex(Current.Position.X, Current.Position.Y, 0.0);
				continue;
			}

			ToPrev /= LenPrev;
			ToNext /= LenNext;

			// Calculate corner angle
			const double Dot = FVector2D::DotProduct(ToPrev, ToNext);
			const double CrossZ = Math::PerpDot(ToPrev, ToNext);

			// Clamp dot product to valid range
			const double ClampedDot = FMath::Clamp(Dot, -1.0, 1.0);
			const double HalfAngle = FMath::Acos(ClampedDot) / 2.0;

			if (FMath::Abs(HalfAngle) < Math::FuzzyEpsilon ||
				FMath::Abs(HalfAngle - PI / 2.0) < Math::FuzzyEpsilon)
			{
				// Straight line or 180 degree turn - no fillet possible
				Result.AddVertex(Current.Position.X, Current.Position.Y, 0.0);
				continue;
			}

			// Calculate fillet parameters
			const double Radius = Current.CornerRadius;
			const double TanHalfAngle = FMath::Tan(HalfAngle);
			double TangentLength = Radius / TanHalfAngle;

			// Clamp tangent length to available space
			const double MaxTangent = FMath::Min(LenPrev, LenNext) * 0.4; // Use 40% max of available length
			if (TangentLength > MaxTangent)
			{
				TangentLength = MaxTangent;
			}

			// Calculate arc start and end points
			const FVector2D ArcStart = CurrPos + ToPrev * TangentLength;
			const FVector2D ArcEnd = CurrPos + ToNext * TangentLength;

			// Determine if arc is clockwise or counter-clockwise based on turn direction
			const bool bIsRightTurn = CrossZ < 0.0;

			// Calculate bulge for the arc
			// For a fillet, the arc sweep is (PI - angle between vectors)
			// Bulge = tan(sweep_angle / 4)
			const double FullAngle = FMath::Acos(ClampedDot);
			const double SweepAngle = PI - FullAngle;
			double Bulge = FMath::Tan(SweepAngle / 4.0);

			// Negate bulge for clockwise arcs
			if (bIsRightTurn)
			{
				Bulge = -Bulge;
			}

			// Add arc start point with bulge, then arc end point
			Result.AddVertex(ArcStart.X, ArcStart.Y, Bulge);
			// Note: We'll add the arc end point but the next segment will handle it
			// or if it's the only corner point needed
			if (i == Points.Num() - 1 || !Points[NextIdx].bIsCorner)
			{
				Result.AddVertex(ArcEnd.X, ArcEnd.Y, 0.0);
			}
		}

		return Result;
	}

	TArray<FContourResult3D> FContourUtils::ComputeOffsetContours(
		const TArray<FInputPoint>& InputPoints,
		bool bClosed,
		double OffsetDistance,
		const FPCGExCCArcTessellationSettings& TessellationSettings,
		const FPCGExCCOffsetOptions& OffsetOptions)
	{
		TArray<FContourResult3D> Results;

		if (InputPoints.Num() < 2)
		{
			return Results;
		}

		// Step 1: Process corners to create the initial polyline with arcs
		FPolyline Polyline = ProcessCorners(InputPoints, bClosed);

		if (Polyline.VertexCount() < 2)
		{
			return Results;
		}

		// Step 2: Compute parallel offset(s)
		TArray<FPolyline> OffsetPolylines = PCGExCavalier::Offset::ParallelOffset(Polyline, OffsetDistance, OffsetOptions);

		// Step 3: Tessellate each offset polyline and convert to 3D
		for (const FPolyline& OffsetPline : OffsetPolylines)
		{
			// Tessellate arcs to line segments
			FPolyline Tessellated = OffsetPline.Tessellated(TessellationSettings);

			// Convert to 3D with Z interpolation
			FContourResult3D Result3D = ConvertTo3D(Tessellated, InputPoints, OffsetPline.IsClosed());

			if (Result3D.Positions.Num() > 0)
			{
				Results.Add(MoveTemp(Result3D));
			}
		}

		return Results;
	}
}
