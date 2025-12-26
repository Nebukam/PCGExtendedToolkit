// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#include "Core/PCGExCCPolyline.h"

namespace PCGExCavalier
{
	//~ FContourPolyline Factory Methods

	FPolyline FPolyline::FromPoints(const TArray<FVector2D>& Points, bool bClosed)
	{
		FPolyline Result(bClosed);
		Result.Vertices.Reserve(Points.Num());

		for (const FVector2D& Point : Points)
		{
			Result.AddVertex(Point.X, Point.Y, 0.0);
		}

		return Result;
	}

	FPolyline FPolyline::FromVectors(const TArray<FVector>& Vectors, bool bClosed)
	{
		FPolyline Result(bClosed);
		Result.Vertices.Reserve(Vectors.Num());

		for (const FVector& Vec : Vectors)
		{
			Result.AddVertex(Vec.X, Vec.Y, 0.0);
		}

		return Result;
	}

	FPolyline FPolyline::FromTransforms(const TConstPCGValueRange<FTransform>& Transforms, bool bClosed)
	{
		FPolyline Result(bClosed);
		Result.Vertices.Reserve(Transforms.Num());

		for (const FTransform& Transform : Transforms)
		{
			const FVector Loc = Transform.GetLocation();
			Result.AddVertex(Loc.X, Loc.Y, 0.0);
		}

		return Result;
	}

	FPolyline FPolyline::FromInputPoints(const TArray<FInputPoint>& Points, bool bClosed)
	{
		return FContourUtils::ProcessCorners(Points, bClosed);
	}

	//~ FContourPolyline Basic Accessors

	int32 FPolyline::SegmentCount() const
	{
		const int32 VC = Vertices.Num();
		if (VC < 2)
		{
			return 0;
		}
		return bIsClosed ? VC : VC - 1;
	}

	const FVertex& FPolyline::GetVertexWrapped(int32 Index) const
	{
		const int32 VC = Vertices.Num();
		check(VC > 0);
		const int32 WrappedIndex = ((Index % VC) + VC) % VC;
		return Vertices[WrappedIndex];
	}

	//~ FContourPolyline Vertex Manipulation

	void FPolyline::AddVertex(const FVertex& Vertex)
	{
		Vertices.Add(Vertex);
	}

	void FPolyline::AddVertex(double X, double Y, double Bulge)
	{
		Vertices.Add(FVertex(X, Y, Bulge));
	}

	void FPolyline::AddOrReplaceVertex(const FVertex& Vertex, double PosEqualEps)
	{
		if (Vertices.Num() > 0)
		{
			FVertex& Last = Vertices.Last();
			if (FMath::Abs(Last.X - Vertex.X) < PosEqualEps &&
				FMath::Abs(Last.Y - Vertex.Y) < PosEqualEps)
			{
				// Replace bulge only
				Last.Bulge = Vertex.Bulge;
				return;
			}
		}
		Vertices.Add(Vertex);
	}

	void FPolyline::SetLastVertex(const FVertex& Vertex)
	{
		check(Vertices.Num() > 0);
		Vertices.Last() = Vertex;
	}

	FVertex FPolyline::RemoveLastVertex()
	{
		check(Vertices.Num() > 0);
		return Vertices.Pop();
	}

	void FPolyline::Clear()
	{
		Vertices.Empty();
	}

	//~ FContourPolyline Index Utilities

	int32 FPolyline::NextWrappingIndex(int32 Index) const
	{
		const int32 Next = Index + 1;
		return (Next >= Vertices.Num()) ? 0 : Next;
	}

	int32 FPolyline::PrevWrappingIndex(int32 Index) const
	{
		return (Index == 0) ? Vertices.Num() - 1 : Index - 1;
	}

	//~ FContourPolyline Geometric Properties

	double FPolyline::PathLength() const
	{
		double Total = 0.0;

		ForEachSegment([&Total](const FVertex& V1, const FVertex& V2)
		{
			Total += Math::SegmentArcLength(V1, V2);
		});

		return Total;
	}

	double FPolyline::Area() const
	{
		if (!bIsClosed || Vertices.Num() < 3)
		{
			return 0.0;
		}

		double DoubleTotalArea = 0.0;

		ForEachSegment([&DoubleTotalArea](const FVertex& V1, const FVertex& V2)
		{
			// Shoelace formula contribution
			DoubleTotalArea += V1.X * V2.Y - V1.Y * V2.X;

			if (!V1.IsLine())
			{
				// Add arc segment area
				const double B = FMath::Abs(V1.Bulge);
				const double SweepAngle = Math::AngleFromBulge(B);
				const FVector2D ChordVec = V2.GetPosition() - V1.GetPosition();
				const double TriangleBase = ChordVec.Size();
				const double Radius = TriangleBase * ((B * B + 1.0) / (4.0 * B));
				const double Sagitta = B * TriangleBase / 2.0;
				const double TriangleHeight = Radius - Sagitta;
				const double DoubleSectorArea = SweepAngle * Radius * Radius;
				const double DoubleTriangleArea = TriangleBase * TriangleHeight;
				double DoubleArcArea = DoubleSectorArea - DoubleTriangleArea;

				if (V1.Bulge < 0.0)
				{
					DoubleArcArea = -DoubleArcArea;
				}

				DoubleTotalArea += DoubleArcArea;
			}
		});

		return DoubleTotalArea / 2.0;
	}

	EPCGExCCOrientation FPolyline::Orientation() const
	{
		if (!bIsClosed)
		{
			return EPCGExCCOrientation::Open;
		}

		return Area() < 0.0 ? EPCGExCCOrientation::Clockwise : EPCGExCCOrientation::CounterClockwise;
	}

	bool FPolyline::GetExtents(FBox2D& OutBox) const
	{
		if (Vertices.Num() < 2)
		{
			return false;
		}

		OutBox = FBox2D(FVector2D(Vertices[0].X, Vertices[0].Y), FVector2D(Vertices[0].X, Vertices[0].Y));

		ForEachSegment([&OutBox](const FVertex& V1, const FVertex& V2)
		{
			if (V1.IsLine())
			{
				// Just check endpoint
				OutBox += FVector2D(V2.X, V2.Y);
			}
			else
			{
				// Arc - need to check if it crosses axis-aligned extremes
				const Math::FArcGeometry Arc = Math::ComputeArcRadiusAndCenter(V1, V2);
				if (!Arc.bValid)
				{
					OutBox += FVector2D(V2.X, V2.Y);
					return;
				}

				const double StartAngle = Math::Angle(Arc.Center, V1.GetPosition());
				const double EndAngle = Math::Angle(Arc.Center, V2.GetPosition());
				const double SweepAngle = Math::DeltaAngleSigned(StartAngle, EndAngle, V1.Bulge < 0.0);

				auto CrossesAngle = [StartAngle, SweepAngle](double TestAngle) -> bool
				{
					return Math::AngleIsWithinSweep(TestAngle, StartAngle, SweepAngle);
				};

				// Check for axis crossings
				if (CrossesAngle(PI))
				{
					OutBox.Min.X = FMath::Min(OutBox.Min.X, Arc.Center.X - Arc.Radius);
				}
				if (CrossesAngle(PI * 1.5))
				{
					OutBox.Min.Y = FMath::Min(OutBox.Min.Y, Arc.Center.Y - Arc.Radius);
				}
				if (CrossesAngle(0.0))
				{
					OutBox.Max.X = FMath::Max(OutBox.Max.X, Arc.Center.X + Arc.Radius);
				}
				if (CrossesAngle(PI * 0.5))
				{
					OutBox.Max.Y = FMath::Max(OutBox.Max.Y, Arc.Center.Y + Arc.Radius);
				}

				// Always include endpoints
				OutBox += FVector2D(V1.X, V1.Y);
				OutBox += FVector2D(V2.X, V2.Y);
			}
		});

		return true;
	}

	//~ FContourPolyline Segment Iteration

	void FPolyline::ForEachSegment(FSegmentVisitor Visitor) const
	{
		const int32 VC = Vertices.Num();
		if (VC < 2)
		{
			return;
		}

		const int32 SegCount = bIsClosed ? VC : VC - 1;

		for (int32 i = 0; i < SegCount; ++i)
		{
			const int32 NextI = (i + 1) % VC;
			Visitor(Vertices[i], Vertices[NextI]);
		}
	}

	//~ FContourPolyline Transformations

	FPolyline FPolyline::Inverted() const
	{
		FPolyline Result(bIsClosed);
		Result.Vertices.Reserve(Vertices.Num());

		if (Vertices.Num() == 0)
		{
			return Result;
		}

		// Last vertex becomes first with zero bulge
		Result.AddVertex(Vertices.Last().X, Vertices.Last().Y, 0.0);

		// Iterate in reverse, taking bulge from previous (now next) vertex
		for (int32 i = Vertices.Num() - 2; i >= 0; --i)
		{
			const FVertex& V = Vertices[i];
			Result.AddVertex(V.X, V.Y, -Vertices[i + 1].Bulge);
		}

		// Handle closed polyline - first vertex's bulge goes to last
		if (bIsClosed && Vertices.Num() > 0)
		{
			Result.Vertices.Last().Bulge = -Vertices[0].Bulge;
		}

		return Result;
	}

	void FPolyline::Invert()
	{
		*this = Inverted();
	}

	FPolyline FPolyline::WithRedundantVerticesRemoved(double PosEqualEps) const
	{
		FPolyline Result(bIsClosed);

		if (Vertices.Num() < 2)
		{
			Result.Vertices = Vertices;
			return Result;
		}

		Result.Vertices.Reserve(Vertices.Num());

		for (const FVertex& V : Vertices)
		{
			Result.AddOrReplaceVertex(V, PosEqualEps);
		}

		// Handle closed polyline - check if last equals first
		if (bIsClosed && Result.Vertices.Num() >= 2)
		{
			const FVertex& First = Result.Vertices[0];
			const FVertex& Last = Result.Vertices.Last();
			if (FMath::Abs(Last.X - First.X) < PosEqualEps &&
				FMath::Abs(Last.Y - First.Y) < PosEqualEps)
			{
				Result.Vertices.Pop();
			}
		}

		return Result;
	}

	bool FPolyline::RemoveRedundantVertices(double PosEqualEps)
	{
		const int32 OrigCount = Vertices.Num();
		*this = WithRedundantVerticesRemoved(PosEqualEps);
		return Vertices.Num() < OrigCount;
	}

	//~ FContourPolyline Arc Tessellation

	FPolyline FPolyline::Tessellated(const FPCGExCCArcTessellationSettings& Settings) const
	{
		FPolyline Result(bIsClosed);

		if (Vertices.Num() < 2)
		{
			Result.Vertices = Vertices;
			return Result;
		}

		// Estimate output size (rough approximation)
		Result.Vertices.Reserve(Vertices.Num() * 2);

		ForEachSegment([&Result, &Settings](const FVertex& V1, const FVertex& V2)
		{
			// Add start vertex (as line, bulge = 0)
			if (Result.Vertices.Num() == 0 ||
				!Result.Vertices.Last().GetPosition().Equals(V1.GetPosition(), Math::FuzzyEpsilon))
			{
				Result.AddVertex(V1.X, V1.Y, 0.0);
			}

			if (V1.IsLine())
			{
				// Line segment - nothing more to add, endpoint will be added by next segment
				return;
			}

			// Arc segment - tessellate
			const Math::FArcGeometry Arc = Math::ComputeArcRadiusAndCenter(V1, V2);
			if (!Arc.bValid)
			{
				return;
			}

			const double ArcLength = Math::SegmentArcLength(V1, V2);
			const int32 SegmentCount = Settings.CalculateSegmentCount(ArcLength);

			const double StartAngle = Math::Angle(Arc.Center, V1.GetPosition());
			const double EndAngle = Math::Angle(Arc.Center, V2.GetPosition());
			const double SweepAngle = Math::DeltaAngleSigned(StartAngle, EndAngle, V1.Bulge < 0.0);

			// Add intermediate points
			for (int32 i = 1; i < SegmentCount; ++i)
			{
				const double T = static_cast<double>(i) / static_cast<double>(SegmentCount);
				const double PointAngle = StartAngle + T * SweepAngle;
				const FVector2D Point = Math::PointOnCircle(Arc.Radius, Arc.Center, PointAngle);
				Result.AddVertex(Point.X, Point.Y, 0.0);
			}
		});

		// Add final vertex for open polylines
		if (!bIsClosed && Vertices.Num() > 0)
		{
			const FVertex& Last = Vertices.Last();
			if (Result.Vertices.Num() == 0 ||
				!Result.Vertices.Last().GetPosition().Equals(Last.GetPosition(), Math::FuzzyEpsilon))
			{
				Result.AddVertex(Last.X, Last.Y, 0.0);
			}
		}

		return Result;
	}

	void FPolyline::Tessellate(const FPCGExCCArcTessellationSettings& Settings)
	{
		*this = Tessellated(Settings);
	}

	//~ FContourPolyline Point Containment

	int32 FPolyline::WindingNumber(const FVector2D& Point) const
	{
		if (!bIsClosed || Vertices.Num() < 3)
		{
			return 0;
		}

		int32 Winding = 0;

		ForEachSegment([&Winding, &Point](const FVertex& V1, const FVertex& V2)
		{
			const FVector2D P1 = V1.GetPosition();
			const FVector2D P2 = V2.GetPosition();

			if (V1.IsLine())
			{
				// Line segment winding contribution
				if (P1.Y <= Point.Y)
				{
					if (P2.Y > Point.Y)
					{
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
						if (!Math::IsLeft(P1, P2, Point))
						{
							--Winding;
						}
					}
				}
			}
			else
			{
				// Arc segment - need more complex handling
				const Math::FArcGeometry Arc = Math::ComputeArcRadiusAndCenter(V1, V2);
				if (!Arc.bValid)
				{
					return;
				}

				const double DistToCenter = FVector2D::Distance(Point, Arc.Center);

				// Check if point could intersect arc
				if (DistToCenter > Arc.Radius + Math::FuzzyEpsilon)
				{
					// Point is outside arc circle, treat as if endpoints connect directly
					// (simplified - full implementation would trace arc crossings)
					if (P1.Y <= Point.Y)
					{
						if (P2.Y > Point.Y)
						{
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
							if (!Math::IsLeft(P1, P2, Point))
							{
								--Winding;
							}
						}
					}
				}
				else
				{
					// Point might intersect arc - use horizontal ray test with arc
					// This is simplified; full implementation would compute actual arc/ray intersections
					const double StartAngle = Math::Angle(Arc.Center, P1);
					const double EndAngle = Math::Angle(Arc.Center, P2);
					const double PointAngle = Math::Angle(Arc.Center, Point);
					const bool bIsCCW = V1.Bulge > 0.0;

					// Check if horizontal ray from point crosses arc
					// Simplified: just use chord approximation
					if (P1.Y <= Point.Y)
					{
						if (P2.Y > Point.Y)
						{
							if (Math::IsLeft(P1, P2, Point) == bIsCCW)
							{
								++Winding;
							}
						}
					}
					else
					{
						if (P2.Y <= Point.Y)
						{
							if (Math::IsLeft(P1, P2, Point) != bIsCCW)
							{
								--Winding;
							}
						}
					}
				}
			}
		});

		return Winding;
	}

	bool FPolyline::ContainsPoint(const FVector2D& Point) const
	{
		return WindingNumber(Point) != 0;
	}

	//~ FContourPolyline Closest Point

	FVector2D FPolyline::ClosestPoint(const FVector2D& Point, int32* OutSegmentIndex, double* OutDistance) const
	{
		if (Vertices.Num() == 0)
		{
			if (OutSegmentIndex) *OutSegmentIndex = -1;
			if (OutDistance) *OutDistance = TNumericLimits<double>::Max();
			return FVector2D::ZeroVector;
		}

		if (Vertices.Num() == 1)
		{
			const FVector2D Result = Vertices[0].GetPosition();
			if (OutSegmentIndex) *OutSegmentIndex = 0;
			if (OutDistance) *OutDistance = FVector2D::Distance(Point, Result);
			return Result;
		}

		FVector2D ClosestPt = FVector2D::ZeroVector;
		double MinDistSq = TNumericLimits<double>::Max();
		int32 ClosestSegIdx = 0;
		int32 CurrentSegIdx = 0;

		ForEachSegment([&](const FVertex& V1, const FVertex& V2)
		{
			const FVector2D SegClosest = Math::SegmentClosestPoint(V1, V2, Point);
			const double DistSq = Math::DistanceSquared(Point, SegClosest);

			if (DistSq < MinDistSq)
			{
				MinDistSq = DistSq;
				ClosestPt = SegClosest;
				ClosestSegIdx = CurrentSegIdx;
			}

			++CurrentSegIdx;
		});

		if (OutSegmentIndex) *OutSegmentIndex = ClosestSegIdx;
		if (OutDistance) *OutDistance = FMath::Sqrt(MinDistSq);
		return ClosestPt;
	}

	//~ FContourPolyline Comparison

	bool FPolyline::FuzzyEquals(const FPolyline& Other, double Epsilon) const
	{
		if (bIsClosed != Other.bIsClosed || Vertices.Num() != Other.Vertices.Num())
		{
			return false;
		}

		for (int32 i = 0; i < Vertices.Num(); ++i)
		{
			if (!Vertices[i].FuzzyEquals(Other.Vertices[i], Epsilon))
			{
				return false;
			}
		}

		return true;
	}
}
