// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#include "Core/PCGExCCOffset.h"
#include "Core/PCGExCCPolyline.h"

namespace PCGExCavalier
{
	namespace Offset
	{
		namespace Internal
		{
			/**
			 * Raw offset segment representing a parallel-offset line or arc
			 */
			struct FRawOffsetSeg
			{
				FVertex V1;
				FVertex V2;
				FVector2D OrigV2Pos;
				bool bCollapsedArc = false;

				FRawOffsetSeg() = default;

				FRawOffsetSeg(const FVertex& InV1, const FVertex& InV2, const FVector2D& InOrigV2Pos, bool bInCollapsed = false)
					: V1(InV1), V2(InV2), OrigV2Pos(InOrigV2Pos), bCollapsedArc(bInCollapsed)
				{
				}
			};

			/**
			 * Create raw untrimmed offset segments for a polyline
			 */
			TArray<FRawOffsetSeg> CreateRawOffsetSegments(const FPolyline& Polyline, double Offset)
			{
				TArray<FRawOffsetSeg> Result;
				Result.Reserve(Polyline.SegmentCount());

				Polyline.ForEachSegment([&Result, Offset](const FVertex& V1, const FVertex& V2)
				{
					const FVector2D Pos1 = V1.GetPosition();
					const FVector2D Pos2 = V2.GetPosition();

					if (V1.IsLine())
					{
						// Line segment offset
						const FVector2D LineVec = Pos2 - Pos1;
						const FVector2D OffsetVec = Math::UnitPerp(LineVec) * Offset;

						Result.Add(FRawOffsetSeg(
							FVertex(Pos1 + OffsetVec, 0.0),
							FVertex(Pos2 + OffsetVec, 0.0),
							Pos2
						));
					}
					else
					{
						// Arc segment offset
						const Math::FArcGeometry Arc = Math::ComputeArcRadiusAndCenter(V1, V2);
						if (!Arc.bValid)
						{
							// Fallback to line
							const FVector2D LineVec = Pos2 - Pos1;
							const FVector2D OffsetVec = Math::UnitPerp(LineVec) * Offset;
							Result.Add(FRawOffsetSeg(
								FVertex(Pos1 + OffsetVec, 0.0),
								FVertex(Pos2 + OffsetVec, 0.0),
								Pos2
							));
							return;
						}

						const double Offs = V1.Bulge < 0.0 ? Offset : -Offset;
						const double RadiusAfterOffset = Arc.Radius + Offs;

						FVector2D V1ToCenter = Pos1 - Arc.Center;
						FVector2D V2ToCenter = Pos2 - Arc.Center;
						V1ToCenter.Normalize();
						V2ToCenter.Normalize();

						double NewBulge = V1.Bulge;
						bool bCollapsed = false;

						if (RadiusAfterOffset < Math::FuzzyEpsilon)
						{
							// Arc collapsed - convert to line
							NewBulge = 0.0;
							bCollapsed = true;
						}

						Result.Add(FRawOffsetSeg(
							FVertex(Pos1 + V1ToCenter * Offs, NewBulge),
							FVertex(Pos2 + V2ToCenter * Offs, V2.Bulge),
							Pos2,
							bCollapsed
						));
					}
				});

				return Result;
			}

			/**
			 * Compute bulge for arc connection between segments
			 */
			double BulgeForConnection(const FVector2D& ArcCenter, const FVector2D& StartPoint, const FVector2D& EndPoint, bool bIsCCW)
			{
				const double A1 = Math::Angle(ArcCenter, StartPoint);
				const double A2 = Math::Angle(ArcCenter, EndPoint);
				return Math::BulgeFromAngle(Math::DeltaAngleSigned(A1, A2, !bIsCCW));
			}

			/**
			 * Connect two segments with an arc
			 */
			void ConnectWithArc(
				const FRawOffsetSeg& S1,
				const FRawOffsetSeg& S2,
				bool bConnectionArcsCCW,
				FPolyline& Result,
				double PosEqualEps)
			{
				const FVector2D ArcCenter = S1.OrigV2Pos;
				const FVector2D StartPt = S1.V2.GetPosition();
				const FVector2D EndPt = S2.V1.GetPosition();
				const double Bulge = BulgeForConnection(ArcCenter, StartPt, EndPt, bConnectionArcsCCW);

				Result.AddOrReplaceVertex(FVertex(StartPt, Bulge), PosEqualEps);
				Result.AddOrReplaceVertex(FVertex(EndPt, S2.V1.Bulge), PosEqualEps);
			}

			/**
			 * Join two line segments
			 */
			void JoinLineToLine(
				const FRawOffsetSeg& S1,
				const FRawOffsetSeg& S2,
				bool bConnectionArcsCCW,
				FPolyline& Result,
				double PosEqualEps)
			{
				if (S1.bCollapsedArc || S2.bCollapsedArc)
				{
					ConnectWithArc(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
					return;
				}

				const Math::FLineLineIntersect Intr = Math::LineLineIntersection(
					S1.V1.GetPosition(), S1.V2.GetPosition(),
					S2.V1.GetPosition(), S2.V2.GetPosition(),
					PosEqualEps);

				switch (Intr.Type)
				{
				case Math::ELineLineIntersectType::None:
					{
						// Parallel lines - join with half circle
						const FVector2D SP = S1.V2.GetPosition();
						const FVector2D EP = S2.V1.GetPosition();
						const double Bulge = bConnectionArcsCCW ? 1.0 : -1.0;
						Result.AddOrReplaceVertex(FVertex(SP, Bulge), PosEqualEps);
						Result.AddOrReplaceVertex(FVertex(EP, S2.V1.Bulge), PosEqualEps);
					}
					break;

				case Math::ELineLineIntersectType::True:
					Result.AddOrReplaceVertex(FVertex(Intr.Point, 0.0), PosEqualEps);
					break;

				case Math::ELineLineIntersectType::Overlapping:
					Result.AddOrReplaceVertex(FVertex(S1.V2.GetPosition(), 0.0), PosEqualEps);
					break;

				case Math::ELineLineIntersectType::False:
					if (Intr.T1 > 1.0 && (Intr.T2 < 0.0 || Intr.T2 > 1.0))
					{
						// Extend and join with arc
						ConnectWithArc(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
					}
					else
					{
						Result.AddOrReplaceVertex(FVertex(S1.V2.GetPosition(), 0.0), PosEqualEps);
					}
					break;
				}
			}

			/**
			 * Join line to arc segment
			 */
			void JoinLineToArc(
				const FRawOffsetSeg& S1,
				const FRawOffsetSeg& S2,
				bool bConnectionArcsCCW,
				FPolyline& Result,
				double PosEqualEps)
			{
				const Math::FArcGeometry Arc = Math::ComputeArcRadiusAndCenter(S2.V1, S2.V2);
				if (!Arc.bValid)
				{
					JoinLineToLine(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
					return;
				}

				const Math::FLineCircleIntersect Intr = Math::LineCircleIntersection(
					S1.V1.GetPosition(), S1.V2.GetPosition(),
					Arc.Center, Arc.Radius,
					PosEqualEps);

				if (Intr.Count == 0)
				{
					ConnectWithArc(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
					return;
				}

				// Find valid intersection (on both segments)
				FVector2D ValidPt;
				bool bFoundValid = false;

				auto IsValidT = [](double T) { return T >= -Math::FuzzyEpsilon && T <= 1.0 + Math::FuzzyEpsilon; };
				auto IsOnArc = [&S2](const FVector2D& Pt)
				{
					return Math::PointWithinArcSweep(
						Math::ComputeArcRadiusAndCenter(S2.V1, S2.V2).Center,
						S2.V1.GetPosition(), S2.V2.GetPosition(),
						S2.V1.Bulge < 0.0, Pt);
				};

				if (IsValidT(Intr.T1) && IsOnArc(Intr.Point1))
				{
					ValidPt = Intr.Point1;
					bFoundValid = true;
				}
				else if (Intr.Count > 1 && IsValidT(Intr.T2) && IsOnArc(Intr.Point2))
				{
					ValidPt = Intr.Point2;
					bFoundValid = true;
				}

				if (bFoundValid)
				{
					Result.AddOrReplaceVertex(FVertex(ValidPt, S2.V1.Bulge), PosEqualEps);
				}
				else
				{
					ConnectWithArc(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
				}
			}

			/**
			 * Join arc to line segment
			 */
			void JoinArcToLine(
				const FRawOffsetSeg& S1,
				const FRawOffsetSeg& S2,
				bool bConnectionArcsCCW,
				FPolyline& Result,
				double PosEqualEps)
			{
				const Math::FArcGeometry Arc = Math::ComputeArcRadiusAndCenter(S1.V1, S1.V2);
				if (!Arc.bValid)
				{
					JoinLineToLine(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
					return;
				}

				const Math::FLineCircleIntersect Intr = Math::LineCircleIntersection(
					S2.V1.GetPosition(), S2.V2.GetPosition(),
					Arc.Center, Arc.Radius,
					PosEqualEps);

				if (Intr.Count == 0)
				{
					ConnectWithArc(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
					return;
				}

				// Similar logic to JoinLineToArc but reversed
				FVector2D ValidPt;
				bool bFoundValid = false;

				auto IsValidT = [](double T) { return T >= -Math::FuzzyEpsilon && T <= 1.0 + Math::FuzzyEpsilon; };
				auto IsOnArc = [&S1](const FVector2D& Pt)
				{
					return Math::PointWithinArcSweep(
						Math::ComputeArcRadiusAndCenter(S1.V1, S1.V2).Center,
						S1.V1.GetPosition(), S1.V2.GetPosition(),
						S1.V1.Bulge < 0.0, Pt);
				};

				if (IsValidT(Intr.T1) && IsOnArc(Intr.Point1))
				{
					ValidPt = Intr.Point1;
					bFoundValid = true;
				}
				else if (Intr.Count > 1 && IsValidT(Intr.T2) && IsOnArc(Intr.Point2))
				{
					ValidPt = Intr.Point2;
					bFoundValid = true;
				}

				if (bFoundValid)
				{
					Result.AddOrReplaceVertex(FVertex(ValidPt, 0.0), PosEqualEps);
				}
				else
				{
					ConnectWithArc(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
				}
			}

			/**
			 * Join arc to arc segment
			 */
			void JoinArcToArc(
				const FRawOffsetSeg& S1,
				const FRawOffsetSeg& S2,
				bool bConnectionArcsCCW,
				FPolyline& Result,
				double PosEqualEps)
			{
				const Math::FArcGeometry Arc1 = Math::ComputeArcRadiusAndCenter(S1.V1, S1.V2);
				const Math::FArcGeometry Arc2 = Math::ComputeArcRadiusAndCenter(S2.V1, S2.V2);

				if (!Arc1.bValid || !Arc2.bValid)
				{
					JoinLineToLine(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
					return;
				}

				const Math::FCircleCircleIntersect Intr = Math::CircleCircleIntersection(
					Arc1.Center, Arc1.Radius,
					Arc2.Center, Arc2.Radius,
					PosEqualEps);

				if (Intr.Count == 0)
				{
					ConnectWithArc(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
					return;
				}

				auto IsOnArc1 = [&S1, &Arc1](const FVector2D& Pt)
				{
					return Math::PointWithinArcSweep(Arc1.Center, S1.V1.GetPosition(), S1.V2.GetPosition(), S1.V1.Bulge < 0.0, Pt);
				};
				auto IsOnArc2 = [&S2, &Arc2](const FVector2D& Pt)
				{
					return Math::PointWithinArcSweep(Arc2.Center, S2.V1.GetPosition(), S2.V2.GetPosition(), S2.V1.Bulge < 0.0, Pt);
				};

				FVector2D ValidPt;
				bool bFoundValid = false;

				if (IsOnArc1(Intr.Point1) && IsOnArc2(Intr.Point1))
				{
					ValidPt = Intr.Point1;
					bFoundValid = true;
				}
				else if (Intr.Count > 1 && IsOnArc1(Intr.Point2) && IsOnArc2(Intr.Point2))
				{
					ValidPt = Intr.Point2;
					bFoundValid = true;
				}

				if (bFoundValid)
				{
					Result.AddOrReplaceVertex(FVertex(ValidPt, S2.V1.Bulge), PosEqualEps);
				}
				else
				{
					ConnectWithArc(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
				}
			}

			/**
			 * Join two raw offset segments
			 */
			void JoinSegments(
				const FRawOffsetSeg& S1,
				const FRawOffsetSeg& S2,
				bool bConnectionArcsCCW,
				FPolyline& Result,
				double PosEqualEps)
			{
				const bool bS1IsLine = S1.V1.IsLine();
				const bool bS2IsLine = S2.V1.IsLine();

				if (bS1IsLine && bS2IsLine)
				{
					JoinLineToLine(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
				}
				else if (bS1IsLine && !bS2IsLine)
				{
					JoinLineToArc(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
				}
				else if (!bS1IsLine && bS2IsLine)
				{
					JoinArcToLine(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
				}
				else
				{
					JoinArcToArc(S1, S2, bConnectionArcsCCW, Result, PosEqualEps);
				}
			}

			/**
			 * Create raw offset polyline by joining offset segments
			 */
			FPolyline CreateRawOffsetPolyline(
				const FPolyline& Polyline,
				const TArray<FRawOffsetSeg>& RawSegs,
				double Offset,
				double PosEqualEps)
			{
				FPolyline Result(Polyline.IsClosed());

				if (RawSegs.Num() == 0)
				{
					return Result;
				}

				// Determine connection arc direction based on offset direction
				// For positive offset (left of travel direction), connection arcs should be CCW
				// For negative offset (right of travel direction), connection arcs should be CW
				const bool bConnectionArcsCCW = Offset > 0.0;

				Result.Reserve(RawSegs.Num() * 2);

				// Add first segment start point
				Result.AddVertex(RawSegs[0].V1);

				// Join consecutive segments
				for (int32 i = 0; i < RawSegs.Num(); ++i)
				{
					const int32 NextI = (i + 1) % RawSegs.Num();

					// Skip last connection for open polylines
					if (!Polyline.IsClosed() && NextI == 0)
					{
						// Just add the end point of the last segment
						Result.AddOrReplaceVertex(RawSegs[i].V2, PosEqualEps);
						break;
					}

					JoinSegments(RawSegs[i], RawSegs[NextI], bConnectionArcsCCW, Result, PosEqualEps);
				}

				return Result;
			}
		}

		TArray<FPolyline> ParallelOffset(FPolyline& Source, double Offset, const FPCGExCCOffsetOptions& Options)
		{
			TArray<FPolyline> Results;

			if (Source.GetVertices().Num() < 2)
			{
				return Results;
			}

			// Create raw offset segments
			TArray<Offset::Internal::FRawOffsetSeg> RawSegs = Offset::Internal::CreateRawOffsetSegments(Source, Offset);

			if (RawSegs.Num() == 0)
			{
				return Results;
			}

			// Create raw offset polyline
			FPolyline RawOffset = Offset::Internal::CreateRawOffsetPolyline(Source, RawSegs, Offset, Options.PositionEqualEpsilon);

			// For simple cases (open polylines or non-self-intersecting), just return the raw offset
			// Full implementation would include self-intersection detection and slice stitching
			if (RawOffset.VertexCount() >= 2)
			{
				// Clean up redundant vertices
				RawOffset.RemoveRedundantVertices(Options.PositionEqualEpsilon);

				if (RawOffset.VertexCount() >= 2)
				{
					Results.Add(MoveTemp(RawOffset));
				}
			}

			return Results;
		}
	}
}
