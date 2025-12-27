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


	FPolyline FContourUtils::CreateFromInputPoints(const TArray<FInputPoint>& Points, const bool bClosed, const bool bAddFuzziness)
	{
		if (Points.IsEmpty()) { return FPolyline(bClosed); }

		// Determine primary path ID from first point
		const int32 PrimaryPathId = Points[0].PathId;
		FPolyline Result(bClosed, PrimaryPathId);
		Result.Reserve(Points.Num() * 2); // Extra for potential corner arcs
		
		for (int32 i = 0; i < Points.Num(); ++i)
		{
			const FInputPoint& Current = Points[i];
			const FVector2D CurrentPos = Current.GetPosition2D(bAddFuzziness);
			
			if (Current.bIsCorner && Current.CornerRadius > 0.0)
			{
				// Process corner with fillet
				const FInputPoint& Prev = Points[(i - 1 + Points.Num()) % Points.Num()];
				const FInputPoint& Next = Points[(i + 1) % Points.Num()];

				const FVector2D PrevPos = Prev.GetPosition2D(bAddFuzziness);
				const FVector2D NextPos = Next.GetPosition2D(bAddFuzziness);

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

	FPolyline FContourUtils::CreateFromRootPath(const FRootPath& RootPath, const bool bAddFuzziness)
	{
		return CreateFromInputPoints(RootPath.Points, RootPath.bIsClosed, bAddFuzziness);
	}

	FContourResult3D FContourUtils::ConvertTo3D(
		const FPolyline& Polyline2D,
		const TMap<int32, FRootPath>& RootPaths)
	{
		FContourResult3D Result;
		Result.bIsClosed = Polyline2D.IsClosed();
		Result.ContributingPathIds = Polyline2D.GetContributingPathIds();

		const int32 N = Polyline2D.VertexCount();
		if (N == 0)
		{
			return Result;
		}

		Result.Positions.Reserve(N);
		Result.Transforms.Reserve(N);
		Result.Sources.Reserve(N);

		// Helper to get transform from source
		auto GetSourceTransform = [&RootPaths](const FVertexSource& Source) -> const FTransform*
		{
			if (!Source.IsValid()) { return nullptr; }

			const FRootPath* Path = RootPaths.Find(Source.PathId);
			if (!Path) { return nullptr; }

			if (Source.PointIndex < 0 || Source.PointIndex >= Path->Points.Num()) { return nullptr; }


			return &Path->Points[Source.PointIndex].Transform;
		};

		// First pass: fill in vertices with valid sources and compute path distances
		TArray<int32> InvalidIndices;
		TArray<double> PathDistances;
		PathDistances.SetNum(N);
		PathDistances[0] = 0.0;

		// Compute cumulative path distances
		for (int32 i = 1; i < N; ++i)
		{
			const FVector2D Prev = Polyline2D.GetVertex(i - 1).GetPosition();
			const FVector2D Curr = Polyline2D.GetVertex(i).GetPosition();
			PathDistances[i] = PathDistances[i - 1] + FVector2D::Distance(Prev, Curr);
		}

		for (int32 i = 0; i < N; ++i)
		{
			const FVertex& V = Polyline2D.GetVertex(i);
			Result.Sources.Add(V.Source);

			const FTransform* SourceTransform = GetSourceTransform(V.Source);
			if (SourceTransform)
			{
				// Direct lookup
				const FVector SourcePos = SourceTransform->GetLocation();
				const FVector Pos3D(V.GetX(), V.GetY(), SourcePos.Z);
				Result.Positions.Add(Pos3D);

				FTransform OutTransform = *SourceTransform;
				OutTransform.SetLocation(Pos3D);
				Result.Transforms.Add(OutTransform);
			}
			else
			{
				// Placeholder - will interpolate
				Result.Positions.Add(FVector(V.GetX(), V.GetY(), 0.0));
				Result.Transforms.Add(FTransform(FVector(V.GetX(), V.GetY(), 0.0)));
				InvalidIndices.Add(i);
			}
		}

		// Second pass: interpolate for vertices without valid sources using PATH distance
		if (!InvalidIndices.IsEmpty() && N > 1)
		{
			for (int32 InvalidIdx : InvalidIndices)
			{
				// Find nearest valid vertices before and after
				int32 PrevValid = InvalidIndex;
				int32 NextValid = InvalidIndex;

				// Search backward
				for (int32 Offset = 1; Offset < N; ++Offset)
				{
					const int32 Idx = (InvalidIdx - Offset + N) % N;
					if (GetSourceTransform(Polyline2D.GetVertex(Idx).Source))
					{
						PrevValid = Idx;
						break;
					}
				}

				// Search forward
				for (int32 Offset = 1; Offset < N; ++Offset)
				{
					const int32 Idx = (InvalidIdx + Offset) % N;
					if (GetSourceTransform(Polyline2D.GetVertex(Idx).Source))
					{
						NextValid = Idx;
						break;
					}
				}

				if (PrevValid != InvalidIndex && NextValid != InvalidIndex)
				{
					// Interpolate using path distance (cumulative distance along polyline)
					double DistToPrev, DistToNext;

					if (PrevValid < InvalidIdx)
					{
						DistToPrev = PathDistances[InvalidIdx] - PathDistances[PrevValid];
					}
					else
					{
						// Wrapped around (for closed polylines)
						DistToPrev = PathDistances[InvalidIdx] + (PathDistances[N - 1] - PathDistances[PrevValid]);
					}

					if (NextValid > InvalidIdx)
					{
						DistToNext = PathDistances[NextValid] - PathDistances[InvalidIdx];
					}
					else
					{
						// Wrapped around (for closed polylines)
						DistToNext = (PathDistances[N - 1] - PathDistances[InvalidIdx]) + PathDistances[NextValid];
					}

					const double TotalDist = DistToPrev + DistToNext;
					double Alpha = (TotalDist > 1e-9) ? DistToPrev / TotalDist : 0.5;

					const FTransform& PrevTransform = Result.Transforms[PrevValid];
					const FTransform& NextTransform = Result.Transforms[NextValid];

					// Interpolate Z
					const double Z = FMath::Lerp(PrevTransform.GetLocation().Z, NextTransform.GetLocation().Z, Alpha);
					Result.Positions[InvalidIdx].Z = Z;

					// Interpolate transform
					FTransform Interp;
					Interp.Blend(PrevTransform, NextTransform, Alpha);
					Interp.SetLocation(Result.Positions[InvalidIdx]);
					Result.Transforms[InvalidIdx] = Interp;
				}
				else if (PrevValid != InvalidIndex)
				{
					// Only have prev - use it directly
					const double Z = Result.Transforms[PrevValid].GetLocation().Z;
					Result.Positions[InvalidIdx].Z = Z;

					FTransform Copy = Result.Transforms[PrevValid];
					Copy.SetLocation(Result.Positions[InvalidIdx]);
					Result.Transforms[InvalidIdx] = Copy;
				}
				else if (NextValid != InvalidIndex)
				{
					// Only have next - use it directly
					const double Z = Result.Transforms[NextValid].GetLocation().Z;
					Result.Positions[InvalidIdx].Z = Z;

					FTransform Copy = Result.Transforms[NextValid];
					Copy.SetLocation(Result.Positions[InvalidIdx]);
					Result.Transforms[InvalidIdx] = Copy;
				}
				// else: no valid sources, keep default Z=0
			}
		}

		return Result;
	}

	FContourResult3D FContourUtils::ConvertTo3D(
		const FPolyline& Polyline2D,
		const TArray<FInputPoint>& SourcePoints,
		bool bClosed)
	{
		// Build temporary root paths map for legacy API
		TMap<int32, FRootPath> RootPaths;

		if (!SourcePoints.IsEmpty())
		{
			// Group points by PathId
			TMap<int32, TArray<const FInputPoint*>> PointsByPath;
			for (const FInputPoint& Pt : SourcePoints) { PointsByPath.FindOrAdd(Pt.PathId).Add(&Pt); }

			for (auto& Pair : PointsByPath)
			{
				FRootPath Path(Pair.Key, bClosed);
				for (const FInputPoint* Pt : Pair.Value) { Path.Points.Add(*Pt); }
				RootPaths.Add(Pair.Key, MoveTemp(Path));
			}
		}

		return ConvertTo3D(Polyline2D, RootPaths);
	}
}
