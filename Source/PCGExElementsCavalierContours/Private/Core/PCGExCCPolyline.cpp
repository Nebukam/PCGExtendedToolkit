// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#include "Core/PCGExCCPolyline.h"
#include "Core/PCGExCCMath.h"
#include "Details/PCGExCCDetails.h"

namespace PCGExCavalier
{
	// FPolyline - Geometric Properties


	double FPolyline::Area() const
	{
		if (!bClosed || Vertices.Num() < 3)
		{
			return 0.0;
		}

		double Sum = 0.0;
		const int32 N = Vertices.Num();

		for (int32 i = 0; i < N; ++i)
		{
			const FVertex& V1 = Vertices[i];
			const FVertex& V2 = Vertices[(i + 1) % N];

			// Shoelace formula for polygon area
			Sum += (V1.GetX() * V2.GetY()) - (V2.GetX() * V1.GetY());

			// Add arc segment area if present
			if (!V1.IsLine())
			{
				const Math::FArcGeometry Arc = Math::ComputeArcRadiusAndCenter(V1, V2);
				if (Arc.bValid)
				{
					// Arc segment area = (1/2) * r^2 * (theta - sin(theta))
					// where theta is the sweep angle
					const double SweepAngle = 4.0 * FMath::Atan(FMath::Abs(V1.Bulge));
					const double ChordArea = 0.5 * Arc.Radius * Arc.Radius * (SweepAngle - FMath::Sin(SweepAngle));
					Sum += (V1.Bulge > 0.0) ? ChordArea : -ChordArea;
				}
			}
		}

		return Sum * 0.5;
	}

	double FPolyline::PathLength() const
	{
		if (Vertices.Num() < 2)
		{
			return 0.0;
		}

		double Length = 0.0;
		const int32 N = Vertices.Num();
		const int32 SegCount = bClosed ? N : N - 1;

		for (int32 i = 0; i < SegCount; ++i)
		{
			const FVertex& V1 = Vertices[i];
			const FVertex& V2 = Vertices[(i + 1) % N];

			if (V1.IsLine())
			{
				Length += FVector2D::Distance(V1.GetPosition(), V2.GetPosition());
			}
			else
			{
				const Math::FArcGeometry Arc = Math::ComputeArcRadiusAndCenter(V1, V2);
				if (Arc.bValid)
				{
					const double SweepAngle = 4.0 * FMath::Atan(FMath::Abs(V1.Bulge));
					Length += Arc.Radius * SweepAngle;
				}
			}
		}

		return Length;
	}

	FBox2D FPolyline::BoundingBox() const
	{
		if (Vertices.IsEmpty())
		{
			return FBox2D(FVector2D::ZeroVector, FVector2D::ZeroVector);
		}

		FVector2D Min = Vertices[0].GetPosition();
		FVector2D Max = Min;

		const int32 N = Vertices.Num();
		const int32 SegCount = bClosed ? N : N - 1;

		for (int32 i = 0; i < SegCount; ++i)
		{
			const FVertex& V1 = Vertices[i];
			const FVertex& V2 = Vertices[(i + 1) % N];

			// Expand for line endpoint
			Min = FVector2D(FMath::Min(Min.X, V2.GetX()), FMath::Min(Min.Y, V2.GetY()));
			Max = FVector2D(FMath::Max(Max.X, V2.GetX()), FMath::Max(Max.Y, V2.GetY()));

			// Expand for arc extremes if present
			if (!V1.IsLine())
			{
				const Math::FArcGeometry Arc = Math::ComputeArcRadiusAndCenter(V1, V2);
				if (Arc.bValid)
				{
					// Check arc extremes
					const bool bCCW = V1.Bulge > 0.0;
					const FVector2D P1 = V1.GetPosition();
					const FVector2D P2 = V2.GetPosition();

					// Check if arc crosses cardinal directions
					auto CheckExtreme = [&](const FVector2D& Dir)
					{
						const FVector2D ExtremePt = Arc.Center + Dir * Arc.Radius;
						if (Math::PointWithinArcSweep(Arc.Center, P1, P2, !bCCW, ExtremePt, 1e-9))
						{
							Min = FVector2D(FMath::Min(Min.X, ExtremePt.X), FMath::Min(Min.Y, ExtremePt.Y));
							Max = FVector2D(FMath::Max(Max.X, ExtremePt.X), FMath::Max(Max.Y, ExtremePt.Y));
						}
					};

					CheckExtreme(FVector2D(1, 0));  // Right
					CheckExtreme(FVector2D(-1, 0)); // Left
					CheckExtreme(FVector2D(0, 1));  // Top
					CheckExtreme(FVector2D(0, -1)); // Bottom
				}
			}
		}

		return FBox2D(Min, Max);
	}

	EPCGExCCOrientation FPolyline::Orientation() const
	{
		if (!bClosed)
		{
			return EPCGExCCOrientation::Open;
		}

		const double A = Area();
		if (A > 0.0)
		{
			return EPCGExCCOrientation::CounterClockwise;
		}
		if (A < 0.0)
		{
			return EPCGExCCOrientation::Clockwise;
		}

		return EPCGExCCOrientation::Open; // Degenerate case
	}

	int32 FPolyline::WindingNumber(const FVector2D& Point) const
	{
		if (!bClosed || Vertices.Num() < 2)
		{
			return 0;
		}

		int32 Winding = 0;
		const int32 N = Vertices.Num();

		for (int32 i = 0; i < N; ++i)
		{
			const FVertex& V1 = Vertices[i];
			const FVertex& V2 = Vertices[(i + 1) % N];

			const FVector2D P1 = V1.GetPosition();
			const FVector2D P2 = V2.GetPosition();

			if (V1.IsLine())
			{
				// Line segment winding contribution
				if (P1.Y <= Point.Y)
				{
					if (P2.Y > Point.Y)
					{
						// Upward crossing
						if (Math::IsLeft(P1, P2, Point))
						{
							++Winding;
						}
					}
				}
				else
				{
					if (P2.Y <= Point.Y)
					{
						// Downward crossing
						if (!Math::IsLeft(P1, P2, Point))
						{
							--Winding;
						}
					}
				}
			}
			else
			{
				// Arc segment winding contribution
				const Math::FArcGeometry Arc = Math::ComputeArcRadiusAndCenter(V1, V2);
				if (!Arc.bValid) { continue; }

				const bool bIsCW = V1.Bulge < 0.0;
				const double DistToCenter = FVector2D::Distance(Point, Arc.Center);

				// Check if point is inside the arc's circle
				if (DistToCenter > Arc.Radius + Math::FuzzyEpsilon)
				{
					// Point outside circle - treat as line for crossing test
					if (P1.Y <= Point.Y)
					{
						if (P2.Y > Point.Y && Math::IsLeft(P1, P2, Point))
						{
							++Winding;
						}
					}
					else
					{
						if (P2.Y <= Point.Y && !Math::IsLeft(P1, P2, Point))
						{
							--Winding;
						}
					}
				}
				else
				{
					// Point inside or on circle - check arc sweep
					const double StartAngle = Math::Angle(Arc.Center, P1);
					const double EndAngle = Math::Angle(Arc.Center, P2);
					const double PointAngle = Math::Angle(Arc.Center, Point);

					const bool bPointInSweep = Math::PointWithinArcSweep(
						Arc.Center, P1, P2, bIsCW, Point, Math::FuzzyEpsilon);

					if (bPointInSweep)
					{
						// Use crossing number based on arc direction
						if (bIsCW)
						{
							--Winding;
						}
						else
						{
							++Winding;
						}
					}
				}
			}
		}

		return Winding;
	}


	// FPolyline - Transformations


	void FPolyline::Reverse()
	{
		if (Vertices.Num() < 2)
		{
			return;
		}

		// Reverse vertex order
		Algo::Reverse(Vertices);

		// Shift bulge values (each segment now starts from what was its end)
		// For closed polylines, first bulge goes to last position
		// Also negate bulges since arc direction reverses
		TArray<double> NewBulges;
		NewBulges.SetNum(Vertices.Num());

		for (int32 i = 0; i < Vertices.Num(); ++i)
		{
			const int32 OrigIdx = (Vertices.Num() - 1 - i);
			const int32 BulgeSourceIdx = bClosed ? ((OrigIdx - 1 + Vertices.Num()) % Vertices.Num()) : (OrigIdx - 1);

			if (BulgeSourceIdx >= 0 && BulgeSourceIdx < Vertices.Num())
			{
				NewBulges[i] = -Vertices[BulgeSourceIdx].Bulge;
			}
			else
			{
				NewBulges[i] = 0.0;
			}
		}

		for (int32 i = 0; i < Vertices.Num(); ++i)
		{
			Vertices[i].Bulge = NewBulges[i];
		}

		// For open polylines, last vertex bulge should be 0
		if (!bClosed && !Vertices.IsEmpty())
		{
			Vertices.Last().Bulge = 0.0;
		}
	}

	FPolyline FPolyline::Reversed() const
	{
		FPolyline Result = *this;
		Result.Reverse();
		return Result;
	}

	void FPolyline::InvertOrientation()
	{
		Reverse();
	}

	FPolyline FPolyline::InvertedOrientation() const
	{
		return Reversed();
	}

	FPolyline FPolyline::Tessellated(const FPCGExCCArcTessellationSettings& Settings) const
	{
		if (Vertices.Num() < 2)
		{
			return *this;
		}

		FPolyline Result(bClosed, PrimaryPathId);
		Result.AddContributingPaths(ContributingPathIds);

		const int32 N = Vertices.Num();
		const int32 SegCount = bClosed ? N : N - 1;

		for (int32 i = 0; i < SegCount; ++i)
		{
			const FVertex& V1 = Vertices[i];
			const FVertex& V2 = Vertices[(i + 1) % N];

			// Add start vertex (with zero bulge since we're tessellating)
			Result.AddVertex(V1.GetPosition(), 0.0, V1.Source);

			// Tessellate arc if present
			if (!V1.IsLine())
			{
				const Math::FArcGeometry Arc = Math::ComputeArcRadiusAndCenter(V1, V2);
				if (Arc.bValid)
				{
					const double SweepAngle = 4.0 * FMath::Atan(FMath::Abs(V1.Bulge));

					// Determine number of subdivisions
					int32 Subdivisions;
					if (Settings.Mode == EPCGExCCArcTessellationMode::FixedCount)
					{
						Subdivisions = FMath::Max(1, Settings.FixedSegmentCount);
					}
					else
					{
						const double ArcLen = Arc.Radius * SweepAngle;
						Subdivisions = FMath::Clamp(
							FMath::CeilToInt32(ArcLen / Settings.TargetSegmentLength),
							Settings.MinSegmentCount,
							Settings.MaxSegmentCount);
					}

					// Generate intermediate arc points
					const FVector2D StartDir = (V1.GetPosition() - Arc.Center).GetSafeNormal();
					const double DeltaAngle = (V1.Bulge > 0.0 ? 1.0 : -1.0) * SweepAngle / Subdivisions;

					for (int32 j = 1; j < Subdivisions; ++j)
					{
						const double Angle = j * DeltaAngle;
						const double CosA = FMath::Cos(Angle);
						const double SinA = FMath::Sin(Angle);
						const FVector2D RotatedDir(
							StartDir.X * CosA - StartDir.Y * SinA,
							StartDir.X * SinA + StartDir.Y * CosA);
						const FVector2D Pt = Arc.Center + RotatedDir * Arc.Radius;

						// Tessellated vertices use invalid source to trigger interpolation
						// This ensures smooth Z transition along the arc
						Result.AddVertex(Pt, 0.0, FVertexSource::Invalid());
					}
				}
			}
		}

		// Add final vertex for open polylines
		if (!bClosed && !Vertices.IsEmpty())
		{
			Result.AddVertex(Vertices.Last().GetPosition(), 0.0, Vertices.Last().Source);
		}

		return Result;
	}


	// FPolyline - Spatial Index


	FPolyline::FApproxAABBIndex FPolyline::CreateApproxAABBIndex() const
	{
		FApproxAABBIndex Index;
		const int32 N = Vertices.Num();
		const int32 SegCount = bClosed ? N : N - 1;

		Index.Boxes.Reserve(SegCount);

		for (int32 i = 0; i < SegCount; ++i)
		{
			const FVertex& V1 = Vertices[i];
			const FVertex& V2 = Vertices[(i + 1) % N];

			FApproxAABBIndex::FBox Box;

			if (V1.IsLine())
			{
				// Simple line segment bounds
				Box.MinX = FMath::Min(V1.GetX(), V2.GetX());
				Box.MinY = FMath::Min(V1.GetY(), V2.GetY());
				Box.MaxX = FMath::Max(V1.GetX(), V2.GetX());
				Box.MaxY = FMath::Max(V1.GetY(), V2.GetY());
			}
			else
			{
				// Arc bounds - use midpoint for approximation
				const Math::FArcGeometry Arc = Math::ComputeArcRadiusAndCenter(V1, V2);
				if (Arc.bValid)
				{
					// Get arc midpoint
					const FVector2D MidPt = Math::SegmentMidpoint(V1, V2);

					// Start with endpoints
					Box.MinX = FMath::Min(V1.GetX(), V2.GetX());
					Box.MinY = FMath::Min(V1.GetY(), V2.GetY());
					Box.MaxX = FMath::Max(V1.GetX(), V2.GetX());
					Box.MaxY = FMath::Max(V1.GetY(), V2.GetY());

					// Expand for midpoint
					Box.MinX = FMath::Min(Box.MinX, MidPt.X);
					Box.MinY = FMath::Min(Box.MinY, MidPt.Y);
					Box.MaxX = FMath::Max(Box.MaxX, MidPt.X);
					Box.MaxY = FMath::Max(Box.MaxY, MidPt.Y);

					// Expand by sagitta for safety
					const double ChordLen = FVector2D::Distance(V1.GetPosition(), V2.GetPosition());
					const double Sagitta = FMath::Abs(V1.Bulge) * ChordLen * 0.5;
					Box.MinX -= Sagitta;
					Box.MinY -= Sagitta;
					Box.MaxX += Sagitta;
					Box.MaxY += Sagitta;
				}
				else
				{
					// Fallback to line bounds
					Box.MinX = FMath::Min(V1.GetX(), V2.GetX());
					Box.MinY = FMath::Min(V1.GetY(), V2.GetY());
					Box.MaxX = FMath::Max(V1.GetX(), V2.GetX());
					Box.MaxY = FMath::Max(V1.GetY(), V2.GetY());
				}
			}

			Index.Boxes.Add(Box);
		}

		return Index;
	}


	// FPolyline - Closest Point


	FVector2D FPolyline::ClosestPoint(const FVector2D& Point, double* OutDistance) const
	{
		if (Vertices.IsEmpty())
		{
			if (OutDistance)
			{
				*OutDistance = TNumericLimits<double>::Max();
			}
			return FVector2D::ZeroVector;
		}

		if (Vertices.Num() == 1)
		{
			if (OutDistance)
			{
				*OutDistance = FVector2D::Distance(Point, Vertices[0].GetPosition());
			}
			return Vertices[0].GetPosition();
		}

		FVector2D ClosestPt = Vertices[0].GetPosition();
		double MinDistSq = FVector2D::DistSquared(Point, ClosestPt);

		const int32 N = Vertices.Num();
		const int32 SegCount = bClosed ? N : N - 1;

		for (int32 i = 0; i < SegCount; ++i)
		{
			const FVertex& V1 = Vertices[i];
			const FVertex& V2 = Vertices[(i + 1) % N];

			// SegmentClosestPoint handles both line and arc segments
			const FVector2D SegClosest = Math::SegmentClosestPoint(V1, V2, Point);

			const double DistSq = FVector2D::DistSquared(Point, SegClosest);
			if (DistSq < MinDistSq)
			{
				MinDistSq = DistSq;
				ClosestPt = SegClosest;
			}
		}

		if (OutDistance)
		{
			*OutDistance = FMath::Sqrt(MinDistSq);
		}
		return ClosestPt;
	}


	// FContourUtils


	FPolyline FContourUtils::CreateFromInputPoints(const TArray<FInputPoint>& Points, const bool bClosed)
	{
		if (Points.IsEmpty()) { return FPolyline(bClosed); }

		// Determine primary path ID from first point
		const int32 PrimaryPathId = Points[0].PathId;
		FPolyline Result(bClosed, PrimaryPathId);
		Result.Reserve(Points.Num() * 2); // Extra for potential corner arcs

		for (int32 i = 0; i < Points.Num(); ++i)
		{
			const FInputPoint& Current = Points[i];
			const FVector2D CurrentPos = Current.GetPosition2D();

			if (Current.bIsCorner && Current.CornerRadius > 0.0)
			{
				// Process corner with fillet
				const FInputPoint& Prev = Points[(i - 1 + Points.Num()) % Points.Num()];
				const FInputPoint& Next = Points[(i + 1) % Points.Num()];

				const FVector2D PrevPos = Prev.GetPosition2D();
				const FVector2D NextPos = Next.GetPosition2D();

				const FVector2D ToPrev = (PrevPos - CurrentPos).GetSafeNormal();
				const FVector2D ToNext = (NextPos - CurrentPos).GetSafeNormal();

				// Calculate fillet
				const double CrossZ = ToPrev.X * ToNext.Y - ToPrev.Y * ToNext.X;
				if (FMath::Abs(CrossZ) > 1e-9) // Not collinear
				{
					const double DotProd = FVector2D::DotProduct(ToPrev, ToNext);
					const double HalfAngle = FMath::Acos(FMath::Clamp(DotProd, -1.0, 1.0)) * 0.5;
					const double TanHalf = FMath::Tan(HalfAngle);

					if (TanHalf > 1e-9)
					{
						const double TangentLen = Current.CornerRadius / TanHalf;
						const FVector2D ArcStart = CurrentPos + ToPrev * TangentLen;
						const FVector2D ArcEnd = CurrentPos + ToNext * TangentLen;

						// Compute bulge from half angle
						const double Bulge = (CrossZ > 0.0 ? 1.0 : -1.0) * FMath::Tan(HalfAngle * 0.5);

						// Add fillet arc - inherits source from corner point
						Result.AddVertex(ArcStart, Bulge, Current.GetSource());
						Result.AddVertex(ArcEnd, 0.0, Current.GetSource());
						continue;
					}
				}
			}

			// Regular point (or fallback for failed corner)
			Result.AddVertex(CurrentPos, 0.0, Current.GetSource());
		}

		return Result;
	}

	FPolyline FContourUtils::CreateFromRootPath(const FRootPath& RootPath)
	{
		return CreateFromInputPoints(RootPath.Points, RootPath.bIsClosed);
	}

	FContourResult3D FContourUtils::ConvertTo3D(
		const FPolyline& Polyline2D,
		const TMap<int32, FRootPath>& RootPaths,
		const bool bBlendTransforms)
	{
		FContourResult3D Result;
		const int32 N = Polyline2D.VertexCount();

		if (N == 0) { return Result; }

		Result.Positions.Reserve(N);
		Result.Transforms.Reserve(N);

		TSet<int32> InvalidIndices;
		TMap<int32, int32> LastSeenPointIndex; // PathId -> PointIndex tracking

		// Helper to get source transform
		auto GetSourceTransform = [&](const FVertexSource& Source) -> const FTransform*
		{
			if (!Source.HasValidPoint()) { return nullptr; }
			const FRootPath* Path = RootPaths.Find(Source.PathId);
			return (Path && Path->Points.IsValidIndex(Source.PointIndex)) ? &Path->Points[Source.PointIndex].Transform : nullptr;
		};


		// First Pass: Identify unique anchors and mark "splits" or "intersections" as invalid

		for (int32 i = 0; i < N; ++i)
		{
			const FVertex& V = Polyline2D.GetVertex(i);
			Result.Sources.Add(V.Source);
			bool bIsAnchor = false;
			const FTransform* SourceTransform = GetSourceTransform(V.Source);

			if (SourceTransform)
			{
				// Only treat as an anchor if it's the first time we see this PointIndex in a sequence
				// or if it's the very first/last vertex of the polyline.

				int32* LastIdx = LastSeenPointIndex.Find(V.Source.PathId);
				if (!LastIdx || *LastIdx != V.Source.PointIndex || i == 0 || i == N - 1)
				{
					bIsAnchor = true;
					LastSeenPointIndex.Add(V.Source.PathId, V.Source.PointIndex);
				}
			}


			if (bIsAnchor && SourceTransform)
			{
				const FVector Pos = FVector(V.GetX(), V.GetY(), SourceTransform->GetLocation().Z);
				Result.Positions.Add(Pos);

				FTransform NewTransform = *SourceTransform;
				NewTransform.SetLocation(Pos);
				Result.Transforms.Add(NewTransform);
			}
			else
			{
				// Mark as invalid to trigger interpolation in the second pass
				Result.Positions.Add(FVector(V.GetX(), V.GetY(), 0.0));
				Result.Transforms.Add(FTransform(FVector(V.GetX(), V.GetY(), 0.0)));
				InvalidIndices.Add(i);
			}
		}


		if (InvalidIndices.Num() == N) { return Result; } // No anchors found

		// Precompute distances for interpolation
		TArray<double> PathDistances;

		PathDistances.SetNumUninitialized(N);
		double CurrentTotalDist = 0.0;
		PathDistances[0] = 0.0;

		for (int32 i = 0; i < N - 1; ++i)
		{
			CurrentTotalDist += FVector2D::Distance(Polyline2D.GetVertex(i).GetPosition(), Polyline2D.GetVertex(i + 1).GetPosition());
			PathDistances[i + 1] = CurrentTotalDist;
		}


		const double TotalPolyLength = CurrentTotalDist + (Polyline2D.IsClosed() ? FVector2D::Distance(Polyline2D.GetVertex(N - 1).GetPosition(), Polyline2D.GetVertex(0).GetPosition()) : 0.0);

		// Second Pass: Interpolate Z and Transforms for all InvalidIndices
		for (int32 InvalidIdx : InvalidIndices)
		{
			int32 PrevValid = -1;
			int32 NextValid = -1;

			// Find neighbors
			for (int32 j = 1; j < N; ++j)
			{
				int32 idx = (InvalidIdx - j + N) % N;

				if (!InvalidIndices.Contains(idx))
				{
					PrevValid = idx;
					break;
				}
			}

			for (int32 j = 1; j < N; ++j)
			{
				int32 idx = (InvalidIdx + j) % N;

				if (!InvalidIndices.Contains(idx))
				{
					NextValid = idx;
					break;
				}
			}


			if (PrevValid != -1 && NextValid != -1)
			{
				double d1 = PathDistances[InvalidIdx] - PathDistances[PrevValid];
				double d2 = PathDistances[NextValid] - PathDistances[InvalidIdx];

				if (PrevValid > InvalidIdx) d1 += TotalPolyLength;
				if (NextValid < InvalidIdx) d2 += TotalPolyLength;

				const double Alpha = d1 / (d1 + d2);

				// Interpolate Z
				const double Z = FMath::Lerp(Result.Transforms[PrevValid].GetLocation().Z, Result.Transforms[NextValid].GetLocation().Z, Alpha);

				Result.Positions[InvalidIdx].Z = Z;

				// Interpolate full transform for orientation/scale consistency

				FTransform Interpolated = FTransform::Identity;
				Interpolated.Blend(Result.Transforms[PrevValid], Result.Transforms[NextValid], Alpha);
				Interpolated.SetLocation(Result.Positions[InvalidIdx]);
				Result.Transforms[InvalidIdx] = Interpolated;
			}
		}

		return Result;
	}
}
