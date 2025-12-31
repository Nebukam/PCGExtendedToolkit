// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#include "Core/PCGExCCOffset.h"
#include "Core/PCGExCCMath.h"
#include "Core/PCGExCCSegmentIntersect.h"

namespace PCGExCavalier::Offset
{
	// FGridSpatialIndex Implementation


	void FGridSpatialIndex::Build(const FPolyline& Polyline, double PosEqualEps)
	{
		const int32 N = Polyline.VertexCount();
		const int32 SegCount = Polyline.IsClosed() ? N : N - 1;

		if (SegCount < 1)
		{
			CachedSegments.Empty();
			Cells.Empty();
			return;
		}

		// Pre-allocate cached segments
		CachedSegments.SetNumUninitialized(SegCount);

		// First pass: compute all segment AABBs and find world bounds
		WorldMinX = WorldMinY = TNumericLimits<double>::Max();
		WorldMaxX = WorldMaxY = TNumericLimits<double>::Lowest();

		for (int32 i = 0; i < SegCount; ++i)
		{
			const FVertex& V1 = Polyline.GetVertex(i);
			const FVertex& V2 = Polyline.GetVertexWrapped(i + 1);

			FCachedSegment& Seg = CachedSegments[i];
			Seg.bIsArc = !V1.IsLine(PosEqualEps);

			// Compute base AABB from endpoints
			Seg.MinX = FMath::Min(V1.GetX(), V2.GetX());
			Seg.MinY = FMath::Min(V1.GetY(), V2.GetY());
			Seg.MaxX = FMath::Max(V1.GetX(), V2.GetX());
			Seg.MaxY = FMath::Max(V1.GetY(), V2.GetY());

			if (Seg.bIsArc)
			{
				// Cache arc geometry
				Seg.Arc = Math::ComputeArcRadiusAndCenter(V1, V2);
				Seg.bArcValid = Seg.Arc.bValid;

				if (Seg.bArcValid)
				{
					// Expand AABB for arc using midpoint + sagitta
					const FVector2D MidPt = Math::SegmentMidpoint(V1, V2);
					Seg.MinX = FMath::Min(Seg.MinX, MidPt.X);
					Seg.MinY = FMath::Min(Seg.MinY, MidPt.Y);
					Seg.MaxX = FMath::Max(Seg.MaxX, MidPt.X);
					Seg.MaxY = FMath::Max(Seg.MaxY, MidPt.Y);

					const double ChordLen = FVector2D::Distance(V1.GetPosition(), V2.GetPosition());
					const double Sagitta = FMath::Abs(V1.Bulge) * ChordLen * 0.5;
					Seg.MinX -= Sagitta;
					Seg.MinY -= Sagitta;
					Seg.MaxX += Sagitta;
					Seg.MaxY += Sagitta;
				}
			}

			// Update world bounds
			WorldMinX = FMath::Min(WorldMinX, Seg.MinX);
			WorldMinY = FMath::Min(WorldMinY, Seg.MinY);
			WorldMaxX = FMath::Max(WorldMaxX, Seg.MaxX);
			WorldMaxY = FMath::Max(WorldMaxY, Seg.MaxY);
		}

		// Compute optimal grid size (targeting ~4-8 segments per cell on average)
		const double WorldWidth = WorldMaxX - WorldMinX;
		const double WorldHeight = WorldMaxY - WorldMinY;

		if (WorldWidth < PosEqualEps || WorldHeight < PosEqualEps)
		{
			// Degenerate case - single cell
			GridSizeX = GridSizeY = 1;
			CellWidth = WorldWidth + PosEqualEps;
			CellHeight = WorldHeight + PosEqualEps;
		}
		else
		{
			// Target average of 4-8 segments per cell
			const double TargetCellCount = FMath::Max(1.0, static_cast<double>(SegCount) / 6.0);
			const double CellsPerAxis = FMath::Sqrt(TargetCellCount);

			GridSizeX = FMath::Clamp(FMath::CeilToInt32(CellsPerAxis * WorldWidth / FMath::Max(WorldWidth, WorldHeight)), 1, 256);
			GridSizeY = FMath::Clamp(FMath::CeilToInt32(CellsPerAxis * WorldHeight / FMath::Max(WorldWidth, WorldHeight)), 1, 256);

			CellWidth = WorldWidth / GridSizeX;
			CellHeight = WorldHeight / GridSizeY;
		}

		InvCellWidth = 1.0 / CellWidth;
		InvCellHeight = 1.0 / CellHeight;

		// Allocate grid cells
		const int32 TotalCells = GridSizeX * GridSizeY;
		Cells.SetNum(TotalCells);
		for (TArray<int32>& Cell : Cells)
		{
			Cell.Reset();
		}

		// Second pass: insert segments into grid cells
		for (int32 i = 0; i < SegCount; ++i)
		{
			const FCachedSegment& Seg = CachedSegments[i];

			const int32 CellMinX = FMath::Clamp(static_cast<int32>((Seg.MinX - WorldMinX) * InvCellWidth), 0, GridSizeX - 1);
			const int32 CellMinY = FMath::Clamp(static_cast<int32>((Seg.MinY - WorldMinY) * InvCellHeight), 0, GridSizeY - 1);
			const int32 CellMaxX = FMath::Clamp(static_cast<int32>((Seg.MaxX - WorldMinX) * InvCellWidth), 0, GridSizeX - 1);
			const int32 CellMaxY = FMath::Clamp(static_cast<int32>((Seg.MaxY - WorldMinY) * InvCellHeight), 0, GridSizeY - 1);

			for (int32 CellY = CellMinY; CellY <= CellMaxY; ++CellY)
			{
				for (int32 CellX = CellMinX; CellX <= CellMaxX; ++CellX)
				{
					Cells[CellY * GridSizeX + CellX].Add(i);
				}
			}
		}
	}


	// Internal Helper Functions


	namespace Internal
	{
		// Helper: Check if parametric t represents a false intersect
		FORCEINLINE bool IsFalseIntersect(double T)
		{
			return T < 0.0 || T > 1.0;
		}

		// Helper: Bulge for connection arc
		FORCEINLINE double BulgeForConnection(
			const FVector2D& ArcCenter,
			const FVector2D& StartPoint,
			const FVector2D& EndPoint,
			bool bIsCCW)
		{
			const double A1 = Math::Angle(ArcCenter, StartPoint);
			const double A2 = Math::Angle(ArcCenter, EndPoint);
			return Math::BulgeFromAngle(Math::DeltaAngleSigned(A1, A2, !bIsCCW));
		}


		// Segment Split


		FSegSplitResult SegSplitAtPoint(
			const FVertex& V1,
			const FVertex& V2,
			const FVector2D& PointOnSeg,
			double PosEqualEps)
		{
			FSegSplitResult Result;

			// Line segment - no bulge modification needed
			if (V1.IsLine())
			{
				Result.UpdatedStart = V1;
				Result.SplitVertex = FVertex(PointOnSeg, 0.0, V1.Source);
				return Result;
			}

			const FVector2D Pos1 = V1.GetPosition();
			const FVector2D Pos2 = V2.GetPosition();

			// Degenerate cases
			if (Pos1.Equals(Pos2, PosEqualEps) || Pos1.Equals(PointOnSeg, PosEqualEps))
			{
				Result.UpdatedStart = FVertex(PointOnSeg, 0.0, V1.Source);
				Result.SplitVertex = FVertex(PointOnSeg, V1.Bulge, V1.Source);
				return Result;
			}

			if (Pos2.Equals(PointOnSeg, PosEqualEps))
			{
				Result.UpdatedStart = V1;
				Result.SplitVertex = FVertex(Pos2, 0.0, V1.Source);
				return Result;
			}

			// Arc segment - compute proper bulge values
			const Math::FArcGeometry Arc = Math::ComputeArcRadiusAndCenter(V1, V2);
			if (!Arc.bValid)
			{
				Result.UpdatedStart = V1;
				Result.SplitVertex = FVertex(PointOnSeg, 0.0, V1.Source);
				return Result;
			}

			const double PointAngle = Math::Angle(Arc.Center, PointOnSeg);
			const double ArcStartAngle = Math::Angle(Arc.Center, Pos1);
			const double ArcEndAngle = Math::Angle(Arc.Center, Pos2);

			const double Theta1 = Math::DeltaAngleSigned(ArcStartAngle, PointAngle, V1.Bulge < 0.0);
			const double Bulge1 = Math::BulgeFromAngle(Theta1);

			const double Theta2 = Math::DeltaAngleSigned(PointAngle, ArcEndAngle, V1.Bulge < 0.0);
			const double Bulge2 = Math::BulgeFromAngle(Theta2);

			Result.UpdatedStart = FVertex(Pos1, Bulge1, V1.Source);
			Result.SplitVertex = FVertex(PointOnSeg, Bulge2, V1.Source);
			return Result;
		}


		// Create Raw Offset Segments


		void CreateRawOffsetSegments(const FPolyline& Polyline, double Offset, TArray<FRawOffsetSeg>& OutSegments)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(PCGExCavalier::Offset::Internal::CreateRawOffsetSegments);

			const int32 N = Polyline.VertexCount();
			const int32 SegCount = Polyline.IsClosed() ? N : N - 1;

			OutSegments.SetNumUninitialized(SegCount);

			for (int32 i = 0; i < SegCount; ++i)
			{
				const FVertex& V1 = Polyline.GetVertex(i);
				const FVertex& V2 = Polyline.GetVertexWrapped(i + 1);

				FRawOffsetSeg& Seg = OutSegments[i];
				Seg.OrigV1Pos = V1.GetPosition();
				Seg.OrigV2Pos = V2.GetPosition();
				Seg.bCollapsedArc = false;

				if (V1.IsLine())
				{
					const FVector2D LineV = V2.GetPosition() - V1.GetPosition();
					const FVector2D OffsetV = Math::UnitPerp(LineV) * Offset;

					Seg.V1 = FVertex(V1.GetPosition() + OffsetV, 0.0, V1.Source);
					Seg.V2 = FVertex(V2.GetPosition() + OffsetV, 0.0, V2.Source);
				}
				else
				{
					const Math::FArcGeometry Arc = Math::ComputeArcRadiusAndCenter(V1, V2);
					if (!Arc.bValid)
					{
						const FVector2D LineV = V2.GetPosition() - V1.GetPosition();
						const FVector2D OffsetV = Math::UnitPerp(LineV) * Offset;
						Seg.V1 = FVertex(V1.GetPosition() + OffsetV, 0.0, V1.Source);
						Seg.V2 = FVertex(V2.GetPosition() + OffsetV, 0.0, V2.Source);
					}
					else
					{
						const double Offs = V1.Bulge < 0.0 ? Offset : -Offset;
						const double RadiusAfterOffset = Arc.Radius + Offs;

						const FVector2D V1ToCenter = (V1.GetPosition() - Arc.Center).GetSafeNormal();
						const FVector2D V2ToCenter = (V2.GetPosition() - Arc.Center).GetSafeNormal();

						double NewBulge;
						if (RadiusAfterOffset < 0.0)
						{
							NewBulge = 0.0;
							Seg.bCollapsedArc = true;
						}
						else
						{
							NewBulge = V1.Bulge;
						}

						Seg.V1 = FVertex(V1ToCenter * Offs + V1.GetPosition(), NewBulge, V1.Source);
						Seg.V2 = FVertex(V2ToCenter * Offs + V2.GetPosition(), V2.Bulge, V2.Source);
					}
				}
			}
		}


		// Segment Joining Functions


		FORCEINLINE void ConnectUsingArc(
			const FRawOffsetSeg& S1,
			const FRawOffsetSeg& S2,
			bool bConnectionArcsCCW,
			FPolyline& Result,
			double PosEqualEps)
		{
			const FVector2D ArcCenter = S1.OrigV2Pos;
			const FVector2D SP = S1.V2.GetPosition();
			const FVector2D EP = S2.V1.GetPosition();
			const double Bulge = BulgeForConnection(ArcCenter, SP, EP, bConnectionArcsCCW);
			Result.AddOrReplaceVertex(FVertex(SP, Bulge, S1.V2.Source), PosEqualEps);
			Result.AddOrReplaceVertex(FVertex(EP, S2.V1.Bulge, S2.V1.Source), PosEqualEps);
		}

		void LineLineJoin(
			const FRawOffsetSeg& S1,
			const FRawOffsetSeg& S2,
			bool bConnectionArcsCCW,
			FPolyline& Result,
			double PosEqualEps)
		{
			if (S1.bCollapsedArc || S2.bCollapsedArc)
			{
				ConnectUsingArc(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
				return;
			}

			const FVector2D V1 = S1.V1.GetPosition();
			const FVector2D V2 = S1.V2.GetPosition();
			const FVector2D U1 = S2.V1.GetPosition();
			const FVector2D U2 = S2.V2.GetPosition();

			const Math::FLineLineIntersect Intr = Math::LineLineIntersection(V1, V2, U1, U2, PosEqualEps);

			switch (Intr.Type)
			{
			case Math::ELineLineIntersectType::None:
				{
					const double Bulge = bConnectionArcsCCW ? 1.0 : -1.0;
					Result.AddOrReplaceVertex(FVertex(V2, Bulge, S1.V2.Source), PosEqualEps);
					Result.AddOrReplaceVertex(FVertex(U1, S2.V1.Bulge, S2.V1.Source), PosEqualEps);
				}
				break;

			case Math::ELineLineIntersectType::True:
				{
					const FVector2D IntrPt = Math::PointFromParametric(V1, V2, Intr.T1);
					Result.AddOrReplaceVertex(FVertex(IntrPt, 0.0, S1.V2.Source), PosEqualEps);
				}
				break;

			case Math::ELineLineIntersectType::Overlapping:
				Result.AddOrReplaceVertex(FVertex(V2, 0.0, S1.V2.Source), PosEqualEps);
				break;

			case Math::ELineLineIntersectType::False:
				if (Intr.T1 > 1.0 && IsFalseIntersect(Intr.T2))
				{
					ConnectUsingArc(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
				}
				else
				{
					Result.AddOrReplaceVertex(FVertex(V2, 0.0, S1.V2.Source), PosEqualEps);
					Result.AddOrReplaceVertex(FVertex(U1, S2.V1.Bulge, S2.V1.Source), PosEqualEps);
				}
				break;
			}
		}

		void LineArcJoin(
			const FRawOffsetSeg& S1,
			const FRawOffsetSeg& S2,
			bool bConnectionArcsCCW,
			FPolyline& Result,
			double PosEqualEps)
		{
			if (S1.bCollapsedArc || S2.bCollapsedArc)
			{
				ConnectUsingArc(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
				return;
			}

			const FVector2D V1 = S1.V1.GetPosition();
			const FVector2D V2 = S1.V2.GetPosition();
			const FVertex& U1 = S2.V1;
			const FVertex& U2 = S2.V2;

			const Math::FArcGeometry Arc = Math::ComputeArcRadiusAndCenter(U1, U2);
			if (!Arc.bValid)
			{
				LineLineJoin(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
				return;
			}

			const Math::FLineCircleIntersect Intr = Math::LineCircleIntersection(V1, V2, Arc.Center, Arc.Radius, PosEqualEps);

			auto ProcessIntersect = [&](double T, const FVector2D& Intersect) -> bool
			{
				const bool bTrueLineIntr = !IsFalseIntersect(T);
				const bool bTrueArcIntr = Math::PointWithinArcSweep(
					Arc.Center, U1.GetPosition(), U2.GetPosition(),
					U1.Bulge < 0.0, Intersect, PosEqualEps);

				if (bTrueLineIntr && bTrueArcIntr)
				{
					const double A = Math::Angle(Arc.Center, Intersect);
					const double ArcEndAngle = Math::Angle(Arc.Center, U2.GetPosition());
					const double Theta = Math::DeltaAngle(A, ArcEndAngle);

					if ((Theta > 0.0) == (U1.Bulge > 0.0))
					{
						Result.AddOrReplaceVertex(FVertex(Intersect, Math::BulgeFromAngle(Theta), S1.V2.Source), PosEqualEps);
					}
					else
					{
						Result.AddOrReplaceVertex(FVertex(Intersect, U1.Bulge, S1.V2.Source), PosEqualEps);
					}
					return true;
				}

				if (T > 1.0 && !bTrueArcIntr)
				{
					ConnectUsingArc(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
					return true;
				}

				return false;
			};

			if (Intr.Count == 0)
			{
				ConnectUsingArc(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
			}
			else if (Intr.Count == 1)
			{
				if (!ProcessIntersect(Intr.T1, Intr.Point1))
				{
					Result.AddOrReplaceVertex(FVertex(V2, 0.0, S1.V2.Source), PosEqualEps);
					Result.AddOrReplaceVertex(U1, PosEqualEps);
				}
			}
			else
			{
				const double Dist1 = Math::DistanceSquared(Intr.Point1, S1.OrigV2Pos);
				const double Dist2 = Math::DistanceSquared(Intr.Point2, S1.OrigV2Pos);

				if (Dist1 < Dist2)
				{
					if (!ProcessIntersect(Intr.T1, Intr.Point1))
					{
						Result.AddOrReplaceVertex(FVertex(V2, 0.0, S1.V2.Source), PosEqualEps);
						Result.AddOrReplaceVertex(U1, PosEqualEps);
					}
				}
				else
				{
					if (!ProcessIntersect(Intr.T2, Intr.Point2))
					{
						Result.AddOrReplaceVertex(FVertex(V2, 0.0, S1.V2.Source), PosEqualEps);
						Result.AddOrReplaceVertex(U1, PosEqualEps);
					}
				}
			}
		}

		void ArcLineJoin(
			const FRawOffsetSeg& S1,
			const FRawOffsetSeg& S2,
			bool bConnectionArcsCCW,
			FPolyline& Result,
			double PosEqualEps)
		{
			if (S1.bCollapsedArc || S2.bCollapsedArc)
			{
				ConnectUsingArc(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
				return;
			}

			const FVertex& V1 = S1.V1;
			const FVertex& V2 = S1.V2;
			const FVector2D U1 = S2.V1.GetPosition();
			const FVector2D U2 = S2.V2.GetPosition();

			const Math::FArcGeometry Arc = Math::ComputeArcRadiusAndCenter(V1, V2);
			if (!Arc.bValid)
			{
				LineLineJoin(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
				return;
			}

			const Math::FLineCircleIntersect Intr = Math::LineCircleIntersection(U1, U2, Arc.Center, Arc.Radius, PosEqualEps);

			auto ProcessIntersect = [&](double T, const FVector2D& Intersect) -> bool
			{
				const bool bTrueLineIntr = !IsFalseIntersect(T);
				const bool bTrueArcIntr = Math::PointWithinArcSweep(
					Arc.Center, V1.GetPosition(), V2.GetPosition(),
					V1.Bulge < 0.0, Intersect, PosEqualEps);

				if (bTrueLineIntr && bTrueArcIntr)
				{
					if (Result.VertexCount() > 0)
					{
						const FVertex& PrevVertex = Result.LastVertex();
						if (!PrevVertex.IsLine() && !PrevVertex.GetPosition().Equals(V2.GetPosition(), PosEqualEps))
						{
							const double A = Math::Angle(Arc.Center, Intersect);
							const Math::FArcGeometry PrevArc = Math::ComputeArcRadiusAndCenter(PrevVertex, V2);
							if (PrevArc.bValid)
							{
								const double PrevArcStartAngle = Math::Angle(PrevArc.Center, PrevVertex.GetPosition());
								const double UpdatedTheta = Math::DeltaAngle(PrevArcStartAngle, A);
								if ((UpdatedTheta > 0.0) == (PrevVertex.Bulge > 0.0))
								{
									Result.SetLastVertexBulge(Math::BulgeFromAngle(UpdatedTheta));
								}
							}
						}
					}
					Result.AddOrReplaceVertex(FVertex(Intersect, 0.0, S1.V2.Source), PosEqualEps);
					return true;
				}
				return false;
			};

			if (Intr.Count == 0)
			{
				ConnectUsingArc(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
			}
			else if (Intr.Count == 1)
			{
				if (!ProcessIntersect(Intr.T1, Intr.Point1))
				{
					ConnectUsingArc(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
				}
			}
			else
			{
				const FVector2D OrigPoint = S2.bCollapsedArc ? U1 : S1.OrigV2Pos;
				const double Dist1 = Math::DistanceSquared(Intr.Point1, OrigPoint);
				const double Dist2 = Math::DistanceSquared(Intr.Point2, OrigPoint);

				if (Dist1 < Dist2)
				{
					if (!ProcessIntersect(Intr.T1, Intr.Point1))
					{
						ConnectUsingArc(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
					}
				}
				else
				{
					if (!ProcessIntersect(Intr.T2, Intr.Point2))
					{
						ConnectUsingArc(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
					}
				}
			}
		}

		void ArcArcJoin(
			const FRawOffsetSeg& S1,
			const FRawOffsetSeg& S2,
			bool bConnectionArcsCCW,
			FPolyline& Result,
			double PosEqualEps)
		{
			if (S1.bCollapsedArc || S2.bCollapsedArc)
			{
				ConnectUsingArc(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
				return;
			}

			const FVertex& V1 = S1.V1;
			const FVertex& V2 = S1.V2;
			const FVertex& U1 = S2.V1;
			const FVertex& U2 = S2.V2;

			const Math::FArcGeometry Arc1 = Math::ComputeArcRadiusAndCenter(V1, V2);
			const Math::FArcGeometry Arc2 = Math::ComputeArcRadiusAndCenter(U1, U2);

			if (!Arc1.bValid || !Arc2.bValid)
			{
				ConnectUsingArc(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
				return;
			}

			auto BothArcsSweepPoint = [&](const FVector2D& Point) -> bool
			{
				return Math::PointWithinArcSweep(Arc1.Center, V1.GetPosition(), V2.GetPosition(), V1.Bulge < 0.0, Point, PosEqualEps)
					&& Math::PointWithinArcSweep(Arc2.Center, U1.GetPosition(), U2.GetPosition(), U1.Bulge < 0.0, Point, PosEqualEps);
			};

			auto ProcessIntersect = [&](const FVector2D& Intersect, bool bTrueIntersect)
			{
				if (!bTrueIntersect)
				{
					ConnectUsingArc(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
					return;
				}

				if (Result.VertexCount() > 0)
				{
					const FVertex& PrevVertex = Result.LastVertex();
					if (!PrevVertex.IsLine() && !PrevVertex.GetPosition().Equals(V2.GetPosition(), PosEqualEps))
					{
						const double A1 = Math::Angle(Arc1.Center, Intersect);
						const Math::FArcGeometry PrevArc = Math::ComputeArcRadiusAndCenter(PrevVertex, V2);
						if (PrevArc.bValid)
						{
							const double PrevArcStartAngle = Math::Angle(PrevArc.Center, PrevVertex.GetPosition());
							const double UpdatedTheta = Math::DeltaAngle(PrevArcStartAngle, A1);
							if ((UpdatedTheta > 0.0) == (PrevVertex.Bulge > 0.0))
							{
								Result.SetLastVertexBulge(Math::BulgeFromAngle(UpdatedTheta));
							}
						}
					}
				}

				const double A2 = Math::Angle(Arc2.Center, Intersect);
				const double Arc2EndAngle = Math::Angle(Arc2.Center, U2.GetPosition());
				const double Theta = Math::DeltaAngle(A2, Arc2EndAngle);

				if ((Theta > 0.0) == (U1.Bulge > 0.0))
				{
					Result.AddOrReplaceVertex(FVertex(Intersect, Math::BulgeFromAngle(Theta), S1.V2.Source), PosEqualEps);
				}
				else
				{
					Result.AddOrReplaceVertex(FVertex(Intersect, U1.Bulge, S1.V2.Source), PosEqualEps);
				}
			};

			const Math::FCircleCircleIntersect Intr = Math::CircleCircleIntersection(Arc1.Center, Arc1.Radius, Arc2.Center, Arc2.Radius, PosEqualEps);

			if (Intr.Count == 0)
			{
				ConnectUsingArc(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
			}
			else if (Intr.Count == 1)
			{
				ProcessIntersect(Intr.Point1, BothArcsSweepPoint(Intr.Point1));
			}
			else
			{
				const bool bIntr1Valid = BothArcsSweepPoint(Intr.Point1);
				const bool bIntr2Valid = BothArcsSweepPoint(Intr.Point2);

				if (bIntr1Valid == bIntr2Valid)
				{
					const double Dist1 = Math::DistanceSquared(Intr.Point1, S1.OrigV2Pos);
					const double Dist2 = Math::DistanceSquared(Intr.Point2, S1.OrigV2Pos);
					ProcessIntersect(Dist1 < Dist2 ? Intr.Point1 : Intr.Point2, bIntr1Valid);
				}
				else
				{
					ProcessIntersect(bIntr1Valid ? Intr.Point1 : Intr.Point2, true);
				}
			}
		}

		FORCEINLINE void JoinSegments(
			const FRawOffsetSeg& S1,
			const FRawOffsetSeg& S2,
			bool bConnectionArcsCCW,
			FPolyline& Result,
			double PosEqualEps)
		{
			const bool bS1IsLine = S1.V1.IsLine(PosEqualEps);
			const bool bS2IsLine = S2.V1.IsLine(PosEqualEps);

			if (bS1IsLine && bS2IsLine)
			{
				LineLineJoin(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
			}
			else if (bS1IsLine && !bS2IsLine)
			{
				LineArcJoin(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
			}
			else if (!bS1IsLine && bS2IsLine)
			{
				ArcLineJoin(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
			}
			else
			{
				ArcArcJoin(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
			}
		}


		// Create Raw Offset Polyline


		FPolyline CreateRawOffsetPolyline(
			const FPolyline& OriginalPolyline,
			const TArray<FRawOffsetSeg>& Segments,
			double Offset,
			double PosEqualEps)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(PCGExCavalier::Offset::Internal::CreateRawOffsetPolyline);

			if (Segments.IsEmpty())
			{
				return FPolyline(OriginalPolyline.IsClosed(), OriginalPolyline.GetPrimaryPathId());
			}

			if (Segments.Num() == 1 && Segments[0].bCollapsedArc)
			{
				return FPolyline(OriginalPolyline.IsClosed(), OriginalPolyline.GetPrimaryPathId());
			}

			FPolyline Result(OriginalPolyline.IsClosed(), OriginalPolyline.GetPrimaryPathId());
			Result.AddContributingPaths(OriginalPolyline.GetContributingPathIds());
			Result.Reserve(Segments.Num() * 2);

			const bool bConnectionArcsCCW = Offset < 0.0;

			Result.AddVertex(Segments[0].V1);

			if (Segments.Num() >= 2)
			{
				JoinSegments(Segments[0], Segments[1], bConnectionArcsCCW, Result, PosEqualEps);
			}
			const bool bFirstVertexReplaced = (Result.VertexCount() == 1);

			for (int32 i = 1; i < Segments.Num() - 1; ++i)
			{
				JoinSegments(Segments[i], Segments[i + 1], bConnectionArcsCCW, Result, PosEqualEps);
			}

			if (OriginalPolyline.IsClosed() && Result.VertexCount() > 1)
			{
				const FRawOffsetSeg& LastSeg = Segments.Last();
				const FRawOffsetSeg& FirstSeg = Segments[0];

				FPolyline ClosingResult(false, OriginalPolyline.GetPrimaryPathId());
				ClosingResult.AddVertex(Result.LastVertex());
				JoinSegments(LastSeg, FirstSeg, bConnectionArcsCCW, ClosingResult, PosEqualEps);

				Result.SetVertex(Result.VertexCount() - 1, ClosingResult.GetVertex(0));

				for (int32 i = 1; i < ClosingResult.VertexCount(); ++i)
				{
					Result.AddVertex(ClosingResult.GetVertex(i));
				}

				if (!bFirstVertexReplaced && ClosingResult.VertexCount() > 0)
				{
					const FVector2D UpdatedFirstPos = ClosingResult.LastVertex().GetPosition();
					FVertex& FirstV = Result.GetVertex(0);

					if (FirstV.IsLine())
					{
						FirstV = FVertex(UpdatedFirstPos, FirstV.Bulge, FirstV.Source);
					}
					else if (Result.VertexCount() > 1)
					{
						const Math::FArcGeometry Arc = Math::ComputeArcRadiusAndCenter(FirstV, Result.GetVertex(1));
						if (Arc.bValid)
						{
							const double A1 = Math::Angle(Arc.Center, UpdatedFirstPos);
							const double A2 = Math::Angle(Arc.Center, Result.GetVertex(1).GetPosition());
							const double UpdatedTheta = Math::DeltaAngle(A1, A2);

							if ((UpdatedTheta < 0.0 && FirstV.Bulge > 0.0) || (UpdatedTheta > 0.0 && FirstV.Bulge < 0.0))
							{
								FirstV = FVertex(UpdatedFirstPos, FirstV.Bulge, FirstV.Source);
							}
							else
							{
								FirstV = FVertex(UpdatedFirstPos, Math::BulgeFromAngle(UpdatedTheta), FirstV.Source);
							}
						}
						else
						{
							FirstV = FVertex(UpdatedFirstPos, FirstV.Bulge, FirstV.Source);
						}
					}
				}

				while (Result.VertexCount() > 2)
				{
					const FVector2D LastPos = Result.LastVertex().GetPosition();
					const FVector2D FirstPos = Result.GetVertex(0).GetPosition();
					if (LastPos.Equals(FirstPos, PosEqualEps))
					{
						Result.RemoveLastVertex();
					}
					else
					{
						break;
					}
				}
			}
			else if (!OriginalPolyline.IsClosed() && !Segments.IsEmpty())
			{
				Result.AddOrReplaceVertex(Segments.Last().V2.WithBulge(0.0), PosEqualEps);
			}

			return Result;
		}


		// Self-Intersection Detection (Optimized)


		void FindAllSelfIntersections(
			const FPolyline& Polyline,
			const FGridSpatialIndex& Index,
			double PosEqualEps,
			FIntersectionBuffer& OutIntersections)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(PCGExCavalier::Offset::Internal::FindAllSelfIntersections);

			OutIntersections.Reset();

			const int32 N = Polyline.SegmentCount();
			if (N < 2) return;

			// Use bit array for visited pairs (more cache-efficient than TSet)
			// We only need to track (i,j) where i < j, so we use triangular indexing
			const int64 MaxPairs = (static_cast<int64>(N) * (N - 1)) / 2;
			TBitArray<> VisitedPairs;
			VisitedPairs.Init(false, static_cast<int32>(FMath::Min(MaxPairs, static_cast<int64>(INT32_MAX))));

			auto GetPairIndex = [N](int32 i, int32 j) -> int64
			{
				if (i > j) Swap(i, j);
				return static_cast<int64>(i) * N - (static_cast<int64>(i) * (i + 1)) / 2 + (j - i - 1);
			};

			for (int32 i = 0; i < N; ++i)
			{
				const FVertex& V1 = Polyline.GetVertex(i);
				const FVertex& V2 = Polyline.GetVertexWrapped(i + 1);
				const FCachedSegment& Seg = Index.GetSegment(i);

				Index.Query(Seg.MinX, Seg.MinY, Seg.MaxX, Seg.MaxY, [&](int32 j)
				{
					if (j == i) return;

					if (Polyline.IsClosed())
					{
						if (j == (i + 1) % N || j == (i - 1 + N) % N) return;
					}
					else
					{
						if (j == i + 1 || j == i - 1) return;
					}

					const int64 PairIdx = GetPairIndex(i, j);
					if (PairIdx < VisitedPairs.Num() && VisitedPairs[static_cast<int32>(PairIdx)]) return;
					if (PairIdx < VisitedPairs.Num()) VisitedPairs[static_cast<int32>(PairIdx)] = true;

					const FVertex& U1 = Polyline.GetVertex(j);
					const FVertex& U2 = Polyline.GetVertexWrapped(j + 1);

					const FPlineSegIntersect Intr = PlineSegmentIntersect(V1, V2, U1, U2, PosEqualEps);

					if (Intr.Type == EPlineSegIntersectType::OneIntersect || Intr.Type == EPlineSegIntersectType::TangentIntersect)
					{
						OutIntersections.Add(i, j, Intr.Point1);
					}
					else if (Intr.Type == EPlineSegIntersectType::TwoIntersects)
					{
						OutIntersections.Add(i, j, Intr.Point1);
						OutIntersections.Add(i, j, Intr.Point2);
					}
				});
			}
		}


		// Find Intersects Between Two Polylines


		void FindIntersectsBetween(
			const FPolyline& Pline1,
			const FPolyline& Pline2,
			const FGridSpatialIndex& Index1,
			double PosEqualEps,
			FIntersectionBuffer& OutIntersections)
		{
			OutIntersections.Reset();

			const int32 N2 = Pline2.SegmentCount();
			if (N2 < 1 || Index1.IsEmpty()) return;

			for (int32 i = 0; i < N2; ++i)
			{
				const FVertex& U1 = Pline2.GetVertex(i);
				const FVertex& U2 = Pline2.GetVertexWrapped(i + 1);

				// Compute AABB for segment
				double MinX = FMath::Min(U1.GetX(), U2.GetX());
				double MinY = FMath::Min(U1.GetY(), U2.GetY());
				double MaxX = FMath::Max(U1.GetX(), U2.GetX());
				double MaxY = FMath::Max(U1.GetY(), U2.GetY());

				if (!U1.IsLine())
				{
					const double ChordLen = FVector2D::Distance(U1.GetPosition(), U2.GetPosition());
					const double Sagitta = FMath::Abs(U1.Bulge) * ChordLen * 0.5;
					MinX -= Sagitta;
					MinY -= Sagitta;
					MaxX += Sagitta;
					MaxY += Sagitta;
				}

				// Expand query by epsilon for boundary intersects (matches Rust implementation)
				Index1.Query(MinX - PosEqualEps, MinY - PosEqualEps, MaxX + PosEqualEps, MaxY + PosEqualEps, [&](int32 j)
				{
					const FVertex& V1 = Pline1.GetVertex(j);
					const FVertex& V2 = Pline1.GetVertexWrapped(j + 1);

					const FPlineSegIntersect Intr = PlineSegmentIntersect(V1, V2, U1, U2, PosEqualEps);

					switch (Intr.Type)
					{
					case EPlineSegIntersectType::OneIntersect:
					case EPlineSegIntersectType::TangentIntersect:
						OutIntersections.Add(j, i, Intr.Point1);
						break;

					case EPlineSegIntersectType::TwoIntersects:
						OutIntersections.Add(j, i, Intr.Point1);
						OutIntersections.Add(j, i, Intr.Point2);
						break;

					// FIX: Handle overlapping segments by adding both endpoints as slice points
					// This matches Rust implementation (shape_algorithms_mod.rs lines 419-433)
					case EPlineSegIntersectType::OverlappingLines:
					case EPlineSegIntersectType::OverlappingArcs:
						OutIntersections.Add(j, i, Intr.Point1);
						OutIntersections.Add(j, i, Intr.Point2);
						break;

					default:
						break;
					}
				});
			}
		}


		// Point Validation


		bool PointValidForOffset(
			const FPolyline& OriginalPolyline,
			const FGridSpatialIndex& OrigIndex,
			double Offset,
			const FVector2D& Point,
			double PosEqualEps,
			double OffsetTolerance)
		{
			const double MinDist = FMath::Abs(Offset) - OffsetTolerance;
			const double MinDistSq = MinDist * MinDist;
			const double QueryExpand = FMath::Abs(Offset) + OffsetTolerance;
			bool bValid = true;

			OrigIndex.Query(
				Point.X - QueryExpand, Point.Y - QueryExpand,
				Point.X + QueryExpand, Point.Y + QueryExpand,
				[&](int32 SegIdx)
				{
					if (!bValid) return;

					const FVertex& V1 = OriginalPolyline.GetVertex(SegIdx);
					const FVertex& V2 = OriginalPolyline.GetVertexWrapped(SegIdx + 1);
					const FVector2D ClosestPt = Math::SegmentClosestPoint(V1, V2, Point);
					if (Math::DistanceSquared(Point, ClosestPt) < MinDistSq)
					{
						bValid = false;
					}
				});

			return bValid;
		}


		// Create Slices


		// Helper: Add circle-polyline intersections to lookup
		void AddCirclePolylineIntersections(
			const FPolyline& Pline,
			const FGridSpatialIndex& Index,
			const FVector2D& CircleCenter,
			double CircleRadius,
			TMap<int32, TArray<FVector2D>>& IntersectsLookup,
			double PosEqualEps)
		{
			const double QueryExpand = CircleRadius + PosEqualEps;

			Index.Query(
				CircleCenter.X - QueryExpand, CircleCenter.Y - QueryExpand,
				CircleCenter.X + QueryExpand, CircleCenter.Y + QueryExpand,
				[&](int32 SegIdx)
				{
					const FVertex& V1 = Pline.GetVertex(SegIdx);
					const FVertex& V2 = Pline.GetVertexWrapped(SegIdx + 1);

					if (V1.IsLine())
					{
						const Math::FLineCircleIntersect Intr = Math::LineCircleIntersection(
							V1.GetPosition(), V2.GetPosition(), CircleCenter, CircleRadius, PosEqualEps);

						auto IsValidT = [PosEqualEps](double T) -> bool
						{
							return T > PosEqualEps && T < 1.0 - PosEqualEps;
						};

						if (Intr.Count >= 1)
						{
							if (IsValidT(Intr.T1))
							{
								const FVector2D Pt = V1.GetPosition() + (V2.GetPosition() - V1.GetPosition()) * Intr.T1;
								IntersectsLookup.FindOrAdd(SegIdx).Add(Pt);
							}
							if (Intr.Count == 2 && IsValidT(Intr.T2))
							{
								const FVector2D Pt = V1.GetPosition() + (V2.GetPosition() - V1.GetPosition()) * Intr.T2;
								IntersectsLookup.FindOrAdd(SegIdx).Add(Pt);
							}
						}
					}
					else
					{
						const Math::FArcGeometry Arc = Math::ComputeArcRadiusAndCenter(V1, V2);
						if (Arc.bValid)
						{
							const Math::FCircleCircleIntersect Intr = Math::CircleCircleIntersection(
								Arc.Center, Arc.Radius, CircleCenter, CircleRadius, PosEqualEps);

							auto IsValidArcIntr = [&](const FVector2D& Pt) -> bool
							{
								if (V1.GetPosition().Equals(Pt, PosEqualEps)) return false;
								return Math::PointWithinArcSweep(Arc.Center, V1.GetPosition(), V2.GetPosition(),
								                                 V1.Bulge < 0.0, Pt, PosEqualEps);
							};

							if (Intr.Count >= 1)
							{
								if (IsValidArcIntr(Intr.Point1))
								{
									IntersectsLookup.FindOrAdd(SegIdx).Add(Intr.Point1);
								}
								if (Intr.Count == 2 && IsValidArcIntr(Intr.Point2))
								{
									IntersectsLookup.FindOrAdd(SegIdx).Add(Intr.Point2);
								}
							}
						}
					}
				});
		}

		void CreateSlices(
			const FPolyline& Original,
			const FPolyline& RawOffset,
			const FPolyline& DualRawOffset,
			const FGridSpatialIndex& OrigIndex,
			double Offset,
			double PosEqualEps,
			double OffsetTolerance,
			TArray<FPolylineSlice>& OutSlices)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(PCGExCavalier::Offset::Internal::CreateSlices);

			OutSlices.Reset();
			if (RawOffset.VertexCount() < 2) return;

			// Build spatial index for raw offset
			FGridSpatialIndex RawIndex;
			RawIndex.Build(RawOffset, PosEqualEps);

			// Use pooled buffers for intersection detection
			FIntersectionBuffer SelfIntrBuffer;
			FIntersectionBuffer DualIntrBuffer;

			FindAllSelfIntersections(RawOffset, RawIndex, PosEqualEps, SelfIntrBuffer);
			FindIntersectsBetween(RawOffset, DualRawOffset, RawIndex, PosEqualEps, DualIntrBuffer);

			// Build lookup: segment -> intersection points
			TMap<int32, TArray<FVector2D>> IntersectsLookup;

			// For open polylines, add intersections with circles at original endpoints
			if (!Original.IsClosed())
			{
				const double CircleRadius = FMath::Abs(Offset);
				AddCirclePolylineIntersections(RawOffset, RawIndex, Original.GetVertex(0).GetPosition(), CircleRadius, IntersectsLookup, PosEqualEps);
				AddCirclePolylineIntersections(RawOffset, RawIndex, Original.LastVertex().GetPosition(), CircleRadius, IntersectsLookup, PosEqualEps);
			}

			// Add self-intersections
			for (int32 i = 0; i < SelfIntrBuffer.Num(); ++i)
			{
				const FBasicIntersect& SI = SelfIntrBuffer[i];
				IntersectsLookup.FindOrAdd(SI.StartIndex1).Add(SI.Point);
				IntersectsLookup.FindOrAdd(SI.StartIndex2).Add(SI.Point);
			}

			// Add dual intersections
			for (int32 i = 0; i < DualIntrBuffer.Num(); ++i)
			{
				const FBasicIntersect& DI = DualIntrBuffer[i];
				IntersectsLookup.FindOrAdd(DI.StartIndex1).Add(DI.Point);
			}

			// No intersections - check if entire offset is valid
			if (IntersectsLookup.IsEmpty())
			{
				if (PointValidForOffset(Original, OrigIndex, Offset, RawOffset.GetVertex(0).GetPosition(), PosEqualEps, OffsetTolerance))
				{
					FPolylineSlice Slice;
					Slice.StartIndex = 0;

					if (Original.IsClosed())
					{
						Slice.EndIndexOffset = RawOffset.VertexCount() - 1;
						Slice.UpdatedStart = RawOffset.GetVertex(0);
						Slice.UpdatedEndBulge = RawOffset.LastVertex().Bulge;
						Slice.EndPoint = RawOffset.GetVertex(0).GetPosition();
						Slice.EndSource = RawOffset.LastVertex().Source;
					}
					else
					{
						Slice.EndIndexOffset = RawOffset.VertexCount() - 2;
						Slice.UpdatedStart = RawOffset.GetVertex(0);
						Slice.UpdatedEndBulge = RawOffset.GetVertex(RawOffset.VertexCount() - 2).Bulge;
						Slice.EndPoint = RawOffset.LastVertex().GetPosition();
						Slice.EndSource = RawOffset.LastVertex().Source;
					}
					OutSlices.Add(Slice);
				}
				return;
			}

			// Sort intersections by distance from segment start and deduplicate
			for (auto& Pair : IntersectsLookup)
			{
				const FVector2D StartPos = RawOffset.GetVertex(Pair.Key).GetPosition();
				Pair.Value.Sort([&StartPos](const FVector2D& A, const FVector2D& B)
				{
					return Math::DistanceSquared(A, StartPos) < Math::DistanceSquared(B, StartPos);
				});

				TArray<FVector2D>& IntrList = Pair.Value;
				for (int32 i = IntrList.Num() - 1; i > 0; --i)
				{
					if (IntrList[i].Equals(IntrList[i - 1], PosEqualEps))
					{
						IntrList.RemoveAt(i);
					}
				}
			}

			TArray<int32> SortedSegIndices;
			IntersectsLookup.GetKeys(SortedSegIndices);
			SortedSegIndices.Sort();

			const int32 N = RawOffset.VertexCount();

			// Helper: check if offset slice segment intersects original polyline
			auto IntersectsOriginalPolyline = [&](const FVertex& V1, const FVertex& V2) -> bool
			{
				double MinX = FMath::Min(V1.GetX(), V2.GetX());
				double MinY = FMath::Min(V1.GetY(), V2.GetY());
				double MaxX = FMath::Max(V1.GetX(), V2.GetX());
				double MaxY = FMath::Max(V1.GetY(), V2.GetY());

				if (!V1.IsLine())
				{
					const double ChordLen = FVector2D::Distance(V1.GetPosition(), V2.GetPosition());
					const double Sagitta = FMath::Abs(V1.Bulge) * ChordLen * 0.5;
					MinX -= Sagitta;
					MinY -= Sagitta;
					MaxX += Sagitta;
					MaxY += Sagitta;
				}

				bool bHasIntersect = false;
				constexpr double Fuzz = Math::FuzzyEpsilon;
				OrigIndex.Query(MinX - Fuzz, MinY - Fuzz, MaxX + Fuzz, MaxY + Fuzz, [&](int32 SegIdx)
				{
					if (bHasIntersect) return;

					const FVertex& U1 = Original.GetVertex(SegIdx);
					const FVertex& U2 = Original.GetVertexWrapped(SegIdx + 1);
					const FPlineSegIntersect Intr = PlineSegmentIntersect(V1, V2, U1, U2, PosEqualEps);
					if (Intr.Type != EPlineSegIntersectType::NoIntersect) bHasIntersect = true;
				});
				return bHasIntersect;
			};

			// Full slice validation
			auto SliceIsValid = [&](int32 StartIdx, int32 TraverseCount, const FVector2D& StartPt, const FVector2D& EndPt) -> bool
			{
				const int32 NN = RawOffset.VertexCount();
				const FVertex& StartV = RawOffset.GetVertex(StartIdx);
				const FVertex& StartV2 = RawOffset.GetVertexWrapped(StartIdx + 1);

				const bool bStartAtSegEnd = StartV2.GetPosition().Equals(StartPt, PosEqualEps);
				FVertex UpdatedStartV;
				if (bStartAtSegEnd)
				{
					UpdatedStartV = StartV;
				}
				else
				{
					FSegSplitResult Split = SegSplitAtPoint(StartV, StartV2, StartPt, PosEqualEps);
					UpdatedStartV = Split.SplitVertex;
				}

				if (TraverseCount == 0)
				{
					if (!PointValidForOffset(Original, OrigIndex, Offset, StartPt, PosEqualEps, OffsetTolerance)) return false;
					if (!PointValidForOffset(Original, OrigIndex, Offset, EndPt, PosEqualEps, OffsetTolerance)) return false;

					FSegSplitResult EndSplit = SegSplitAtPoint(UpdatedStartV, StartV2, EndPt, PosEqualEps);
					const FVertex V1 = EndSplit.UpdatedStart;
					const FVertex V2(EndPt, 0.0);
					const FVector2D MidPt = Math::SegmentMidpoint(V1, V2);

					if (!PointValidForOffset(Original, OrigIndex, Offset, MidPt, PosEqualEps, OffsetTolerance)) return false;

					return !IntersectsOriginalPolyline(V1, V2);
				}

				const FVector2D StartMid = Math::SegmentMidpoint(UpdatedStartV, StartV2);
				if (!PointValidForOffset(Original, OrigIndex, Offset, StartMid, PosEqualEps, OffsetTolerance)) return false;

				const int32 EndIdx = (StartIdx + TraverseCount) % NN;
				const FVertex& EndV = RawOffset.GetVertex(EndIdx);
				const FVertex& EndV2 = RawOffset.GetVertexWrapped(EndIdx + 1);
				FSegSplitResult EndSplit = SegSplitAtPoint(EndV, EndV2, EndPt, PosEqualEps);
				const FVertex EndPtV(EndPt, 0.0);
				const FVector2D EndMid = Math::SegmentMidpoint(EndV.WithBulge(EndSplit.UpdatedStart.Bulge), EndPtV);

				if (!PointValidForOffset(Original, OrigIndex, Offset, EndMid, PosEqualEps, OffsetTolerance)) return false;

				FVertex PrevV = UpdatedStartV;
				for (int32 i = 1; i <= TraverseCount; ++i)
				{
					const int32 Idx = (StartIdx + i) % NN;
					const FVertex& CurV = RawOffset.GetVertex(Idx);

					if (!PointValidForOffset(Original, OrigIndex, Offset, PrevV.GetPosition(), PosEqualEps, OffsetTolerance)) return false;
					if (IntersectsOriginalPolyline(PrevV, CurV)) return false;

					PrevV = CurV;
				}

				if (!PointValidForOffset(Original, OrigIndex, Offset, EndPt, PosEqualEps, OffsetTolerance)) return false;
				if (IntersectsOriginalPolyline(PrevV, EndPtV)) return false;

				return true;
			};

			// Validate and add slices between intersections
			auto TryAddSlice = [&](const FVector2D& StartPt, int32 StartIdx, const FVector2D& EndPt, int32 EndIdx)
			{
				int32 TraverseCount = EndIdx - StartIdx;
				if (TraverseCount < 0 && Original.IsClosed()) TraverseCount += N;

				if (TraverseCount < 0) return;

				if (TraverseCount == 0 && Original.IsClosed() && !StartPt.Equals(EndPt, PosEqualEps))
				{
					const FVector2D SegStart = RawOffset.GetVertex(StartIdx).GetPosition();
					if (Math::DistanceSquared(SegStart, StartPt) >= Math::DistanceSquared(SegStart, EndPt)) TraverseCount = N;
				}

				if (TraverseCount == 0 && StartPt.Equals(EndPt, PosEqualEps)) return;

				if (!SliceIsValid(StartIdx, TraverseCount, StartPt, EndPt)) return;

				const FVertex& StartV = RawOffset.GetVertex(StartIdx);
				const FVertex& StartV2 = RawOffset.GetVertexWrapped(StartIdx + 1);

				const bool bStartAtSegEnd = StartV2.GetPosition().Equals(StartPt, PosEqualEps);

				FVertex UpdatedStart;
				if (bStartAtSegEnd)
				{
					if (TraverseCount == 0)
					{
						FSegSplitResult EndSplit = SegSplitAtPoint(StartV, StartV2, EndPt, PosEqualEps);
						UpdatedStart = EndSplit.UpdatedStart;
					}
					else
					{
						UpdatedStart = StartV;
					}
				}
				else
				{
					FSegSplitResult StartSplit = SegSplitAtPoint(StartV, StartV2, StartPt, PosEqualEps);
					if (TraverseCount == 0)
					{
						FSegSplitResult EndSplit = SegSplitAtPoint(StartSplit.SplitVertex, StartV2, EndPt, PosEqualEps);
						UpdatedStart = EndSplit.UpdatedStart;
					}
					else
					{
						UpdatedStart = StartSplit.SplitVertex;
					}
				}

				double UpdatedEndBulge;
				FVertexSource EndSource;
				if (TraverseCount == 0)
				{
					UpdatedEndBulge = UpdatedStart.Bulge;
					EndSource = UpdatedStart.Source;
				}
				else
				{
					const int32 EndSegIdx = (StartIdx + TraverseCount) % N;
					const FVertex& EndV = RawOffset.GetVertex(EndSegIdx);
					const FVertex& EndV2 = RawOffset.GetVertexWrapped(EndSegIdx + 1);
					FSegSplitResult EndSplit = SegSplitAtPoint(EndV, EndV2, EndPt, PosEqualEps);
					UpdatedEndBulge = EndSplit.UpdatedStart.Bulge;

					EndSource = EndPt.Equals(EndV2.GetPosition(), PosEqualEps) ? EndV2.Source : EndV.Source;
				}

				FPolylineSlice Slice;
				Slice.StartIndex = StartIdx;
				Slice.EndIndexOffset = TraverseCount;
				Slice.UpdatedStart = UpdatedStart;
				Slice.EndPoint = EndPt;
				Slice.UpdatedEndBulge = UpdatedEndBulge;
				Slice.EndSource = EndSource;

				OutSlices.Add(Slice);
			};

			// For open polylines, build first slice from vertex 0 to first intersection
			if (!Original.IsClosed() && !SortedSegIndices.IsEmpty())
			{
				const int32 FirstIntrIdx = SortedSegIndices[0];
				const FVector2D& FirstIntr = IntersectsLookup[FirstIntrIdx][0];
				TryAddSlice(RawOffset.GetVertex(0).GetPosition(), 0, FirstIntr, FirstIntrIdx);
			}
			

			// Create slices
			for (int32 i = 0; i < SortedSegIndices.Num(); ++i)
			{
				const int32 SegIdx = SortedSegIndices[i];
				const TArray<FVector2D>& IntrList = IntersectsLookup[SegIdx];

				for (int32 j = 0; j < IntrList.Num() - 1; ++j)
				{
					TryAddSlice(IntrList[j], SegIdx, IntrList[j + 1], SegIdx);
				}

				int32 NextSegIdx = INDEX_NONE;
				for (int32 Idx : SortedSegIndices)
				{
					if (Idx > SegIdx)
					{
						NextSegIdx = Idx;
						break;
					}
				}

				if (NextSegIdx != INDEX_NONE)
				{
					TryAddSlice(IntrList.Last(), SegIdx, IntersectsLookup[NextSegIdx][0], NextSegIdx);
				}
				else if (Original.IsClosed() && !SortedSegIndices.IsEmpty())
				{
					NextSegIdx = SortedSegIndices[0];
					TryAddSlice(IntrList.Last(), SegIdx, IntersectsLookup[NextSegIdx][0], NextSegIdx);
				}
				else
				{
					const int32 LastSegIdx = RawOffset.VertexCount() - 2;
					TryAddSlice(IntrList.Last(), SegIdx, RawOffset.LastVertex().GetPosition(), LastSegIdx);
					break;
				}
			}
		}


		// Stitch Slices


		void StitchSlices(
			const FPolyline& RawOffset,
			const TArray<FPolylineSlice>& Slices,
			bool bIsClosed,
			int32 SourcePathId,
			double JoinEps,
			double PosEqualEps,
			TArray<FPolyline>& OutResults)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(PCGExCavalier::Offset::Internal::StitchSlices);

			OutResults.Reset();
			if (Slices.IsEmpty()) return;

			const int32 N = RawOffset.VertexCount();

			// Special case: single slice
			if (Slices.Num() == 1)
			{
				const FPolylineSlice& Slice = Slices[0];
				FPolyline Pline(false, SourcePathId);

				Pline.AddOrReplaceVertex(Slice.UpdatedStart, PosEqualEps);
				for (int32 i = 1; i <= Slice.EndIndexOffset; ++i)
				{
					FVertex V = RawOffset.GetVertex((Slice.StartIndex + i) % N);
					if (i == Slice.EndIndexOffset) V = V.WithBulge(Slice.UpdatedEndBulge);
					Pline.AddOrReplaceVertex(V, PosEqualEps);
				}

				Pline.AddOrReplaceVertex(FVertex(Slice.EndPoint, 0.0, Slice.EndSource), PosEqualEps);

				if (bIsClosed && Pline.VertexCount() >= 2)
				{
					const FVector2D FirstPos = Pline.GetVertex(0).GetPosition();
					const FVector2D LastPos = Pline.LastVertex().GetPosition();
					if (FirstPos.Equals(LastPos, JoinEps))
					{
						Pline.RemoveLastVertex();
						Pline.SetClosed(true);
					}
				}

				if (Pline.VertexCount() >= 2) OutResults.Add(MoveTemp(Pline));
				return;
			}

			TBitArray<> Visited;
			Visited.Init(false, Slices.Num());

			for (int32 StartIdx = 0; StartIdx < Slices.Num(); ++StartIdx)
			{
				if (Visited[StartIdx]) continue;
				Visited[StartIdx] = true;

				FPolyline Pline(false, SourcePathId);
				const FVector2D InitialStart = Slices[StartIdx].UpdatedStart.GetPosition();
				int32 CurrentIdx = StartIdx;
				int32 LoopCount = 0;

				while (LoopCount++ < Slices.Num() + 1)
				{
					const FPolylineSlice& Slice = Slices[CurrentIdx];

					if (Pline.VertexCount() > 0) Pline.RemoveLastVertex();

					Pline.AddOrReplaceVertex(Slice.UpdatedStart, PosEqualEps);
					for (int32 i = 1; i <= Slice.EndIndexOffset; ++i)
					{
						FVertex V = RawOffset.GetVertex((Slice.StartIndex + i) % N);
						if (i == Slice.EndIndexOffset) V = V.WithBulge(Slice.UpdatedEndBulge);
						Pline.AddOrReplaceVertex(V, PosEqualEps);
					}
					Pline.AddOrReplaceVertex(FVertex(Slice.EndPoint, 0.0, Slice.EndSource), PosEqualEps);

					if (Slice.EndPoint.Equals(InitialStart, JoinEps) && Pline.VertexCount() >= 3)
					{
						Pline.RemoveLastVertex();
						Pline.SetClosed(true);
						break;
					}

					int32 NextIdx = INDEX_NONE;
					int32 BestDist = INT32_MAX;
					for (int32 i = 0; i < Slices.Num(); ++i)
					{
						if (Visited[i]) continue;
						if (!Slices[i].UpdatedStart.GetPosition().Equals(Slice.EndPoint, JoinEps)) continue;

						const int32 Dist = (Slices[i].StartIndex >= Slice.StartIndex)
							                   ? Slices[i].StartIndex - Slice.StartIndex
							                   : N - Slice.StartIndex + Slices[i].StartIndex;
						if (Dist < BestDist)
						{
							BestDist = Dist;
							NextIdx = i;
						}
					}

					if (NextIdx == INDEX_NONE) break;

					Visited[NextIdx] = true;
					CurrentIdx = NextIdx;
				}

				if (Pline.VertexCount() >= 2)
				{
					OutResults.Add(MoveTemp(Pline));
				}
			}
		}
	}


	// Public API


	FPolyline RawParallelOffset(const FPolyline& Polyline, double Offset, double PosEqualEps)
	{
		if (Polyline.VertexCount() < 2)
		{
			return FPolyline(Polyline.IsClosed(), Polyline.GetPrimaryPathId());
		}

		TArray<Internal::FRawOffsetSeg> Segments;
		Internal::CreateRawOffsetSegments(Polyline, Offset, Segments);
		return Internal::CreateRawOffsetPolyline(Polyline, Segments, Offset, PosEqualEps);
	}

	TArray<FPolyline> ParallelOffset(const FPolyline& Polyline, double Offset, const FPCGExCCOffsetOptions& Options)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExCavalier::ParallelOffset);

		TArray<FPolyline> Results;
		if (Polyline.VertexCount() < 2) return Results;

		// Create raw offset segments (reused for both offsets)
		TArray<Internal::FRawOffsetSeg> Segments;
		Internal::CreateRawOffsetSegments(Polyline, Offset, Segments);

		FPolyline RawOffset = Internal::CreateRawOffsetPolyline(Polyline, Segments, Offset, Options.PositionEqualEpsilon);
		if (RawOffset.VertexCount() < 2) return Results;

		if (!Options.bHandleSelfIntersects)
		{
			Results.Add(MoveTemp(RawOffset));
			return Results;
		}

		// Create dual offset
		TArray<Internal::FRawOffsetSeg> DualSegments;
		Internal::CreateRawOffsetSegments(Polyline, -Offset, DualSegments);
		FPolyline DualRawOffset = Internal::CreateRawOffsetPolyline(Polyline, DualSegments, -Offset, Options.PositionEqualEpsilon);

		// Build grid spatial index for original polyline (cached arc geometry)
		FGridSpatialIndex OrigIndex;
		OrigIndex.Build(Polyline, Options.PositionEqualEpsilon);

		// Create slices
		TArray<Internal::FPolylineSlice> Slices;
		Internal::CreateSlices(
			Polyline, RawOffset, DualRawOffset, OrigIndex,
			Offset, Options.PositionEqualEpsilon, Options.OffsetDistanceEpsilon,
			Slices);

		// Stitch slices into result polylines
		Internal::StitchSlices(
			RawOffset, Slices, Polyline.IsClosed(), Polyline.GetPrimaryPathId(),
			Options.SliceJoinEpsilon, Options.PositionEqualEpsilon,
			Results);

		return Results;
	}
}
