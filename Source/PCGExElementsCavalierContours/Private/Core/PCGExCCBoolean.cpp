// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours) - Boolean Operations

#include "Core/PCGExCCBoolean.h"
#include "Core/PCGExCCPolyline.h"

namespace PCGExCavalier
{
	namespace BooleanOps
	{
		namespace Internal
		{
			/** Basic intersection point between two polylines */
			struct FBasicIntersect
			{
				int32 StartIndex1; // Segment start index on polyline 1
				int32 StartIndex2; // Segment start index on polyline 2
				FVector2D Point;

				FBasicIntersect()
					: StartIndex1(0), StartIndex2(0), Point(FVector2D::ZeroVector)
				{
				}

				FBasicIntersect(int32 InIdx1, int32 InIdx2, const FVector2D& InPoint)
					: StartIndex1(InIdx1), StartIndex2(InIdx2), Point(InPoint)
				{
				}
			};

			/** Overlapping segment intersection between two polylines */
			struct FOverlappingIntersect
			{
				int32 StartIndex1;
				int32 StartIndex2;
				FVector2D Point1; // Closer to segment 2 start
				FVector2D Point2; // Farther from segment 2 start

				FOverlappingIntersect()
					: StartIndex1(0), StartIndex2(0)
				{
				}

				FOverlappingIntersect(int32 InIdx1, int32 InIdx2, const FVector2D& P1, const FVector2D& P2)
					: StartIndex1(InIdx1), StartIndex2(InIdx2), Point1(P1), Point2(P2)
				{
				}
			};

			/** Collection of all intersects between two polylines */
			struct FIntersectsCollection
			{
				TArray<FBasicIntersect> BasicIntersects;
				TArray<FOverlappingIntersect> OverlappingIntersects;

				bool HasAnyIntersects() const
				{
					return BasicIntersects.Num() > 0 || OverlappingIntersects.Num() > 0;
				}
			};

			/** Segment intersection type */
			enum class ESegIntersectType
			{
				None,
				OnePoint,
				TwoPoints,
				Tangent,
				OverlappingLines,
				OverlappingArcs
			};

			/** Result of segment-segment intersection */
			struct FSegIntersectResult
			{
				ESegIntersectType Type = ESegIntersectType::None;
				FVector2D Point1;
				FVector2D Point2;
			};

			/** Open polyline slice for boolean operations */
			struct FBooleanSlice
			{
				int32 StartIndex;
				int32 EndIndexOffset; // Number of vertices from start (wrapping)
				FVertex UpdatedStart;
				double UpdatedEndBulge;
				FVector2D EndPoint;
				bool bInvertedDirection;
				bool bSourceIsPline1;
				bool bIsOverlapping;

				FVector2D GetStartPoint() const { return UpdatedStart.GetPosition(); }
			};

			/** Overlapping slice between polylines */
			struct FOverlappingSlice
			{
				TPair<int32, int32> StartIndexes; // (pline1, pline2)
				TPair<int32, int32> EndIndexes;
				FVertex UpdatedStart;
				double UpdatedEndBulge;
				FVector2D EndPoint;
				bool bIsLoop;
				bool bOpposingDirections;
			};

			/** Result of processing polylines for boolean operations */
			struct FProcessedBoolean
			{
				TArray<FOverlappingSlice> OverlappingSlices;
				TArray<FBasicIntersect> Intersects;
				EPCGExCCOrientation Pline1Orientation;
				EPCGExCCOrientation Pline2Orientation;

				bool IsCompletelyOverlapping() const
				{
					return OverlappingSlices.Num() == 1 && OverlappingSlices[0].bIsLoop;
				}

				bool HasOpposingDirections() const
				{
					return Pline1Orientation != Pline2Orientation;
				}

				bool HasAnyIntersects() const
				{
					return Intersects.Num() > 0 || OverlappingSlices.Num() > 0;
				}
			};

			/** Pruned slices ready for stitching */
			struct FPrunedSlices
			{
				TArray<FBooleanSlice> Slices;
				int32 StartOfPline2Slices;
				int32 StartOfPline1OverlappingSlices;
				int32 StartOfPline2OverlappingSlices;
			};

			//=============================================================================
			// Segment Intersection
			//=============================================================================

			/** Compute intersection between two polyline segments */
			FSegIntersectResult ComputeSegmentIntersection(
				const FVertex& V1, const FVertex& V2,
				const FVertex& U1, const FVertex& U2,
				double PosEqualEps)
			{
				FSegIntersectResult Result;

				const FVector2D V1Pos = V1.GetPosition();
				const FVector2D V2Pos = V2.GetPosition();
				const FVector2D U1Pos = U1.GetPosition();
				const FVector2D U2Pos = U2.GetPosition();

				const bool bV1IsLine = V1.IsLine(PosEqualEps);
				const bool bU1IsLine = U1.IsLine(PosEqualEps);

				if (bV1IsLine && bU1IsLine)
				{
					// Line-Line intersection
					const Math::FLineLineIntersect Intr = Math::LineLineIntersection(
						V1Pos, V2Pos, U1Pos, U2Pos, PosEqualEps);

					switch (Intr.Type)
					{
					case Math::ELineLineIntersectType::True:
						Result.Type = ESegIntersectType::OnePoint;
						Result.Point1 = Intr.Point;
						break;
					case Math::ELineLineIntersectType::Overlapping:
						{
							// Compute overlap region
							double T1Start = 0.0, T1End = 1.0;
							double T2Start = Math::DistanceSquared(V1Pos, U1Pos) < PosEqualEps * PosEqualEps ? 0.0 : FVector2D::DotProduct(U1Pos - V1Pos, V2Pos - V1Pos) / FVector2D::DotProduct(V2Pos - V1Pos, V2Pos - V1Pos);
							double T2End = Math::DistanceSquared(V1Pos, U2Pos) < PosEqualEps * PosEqualEps ? 0.0 : FVector2D::DotProduct(U2Pos - V1Pos, V2Pos - V1Pos) / FVector2D::DotProduct(V2Pos - V1Pos, V2Pos - V1Pos);

							if (T2Start > T2End) Swap(T2Start, T2End);

							const double OverlapStart = FMath::Max(T1Start, T2Start);
							const double OverlapEnd = FMath::Min(T1End, T2End);

							if (OverlapEnd > OverlapStart + PosEqualEps)
							{
								Result.Type = ESegIntersectType::OverlappingLines;
								Result.Point1 = Math::PointFromParametric(V1Pos, V2Pos, OverlapStart);
								Result.Point2 = Math::PointFromParametric(V1Pos, V2Pos, OverlapEnd);
							}
						}
						break;
					default:
						break;
					}
				}
				else if (bV1IsLine && !bU1IsLine)
				{
					// Line-Arc intersection
					const Math::FArcGeometry Arc = Math::ComputeArcRadiusAndCenter(U1, U2);
					if (Arc.bValid)
					{
						const Math::FLineCircleIntersect Intr = Math::LineCircleIntersection(
							V1Pos, V2Pos, Arc.Center, Arc.Radius, PosEqualEps);

						TArray<FVector2D> ValidPoints;

						auto IsValidIntersect = [&](const FVector2D& Pt, double T) -> bool
						{
							if (T < -PosEqualEps || T > 1.0 + PosEqualEps) return false;
							return Math::PointWithinArcSweep(Arc.Center, U1Pos, U2Pos, U1.Bulge < 0.0, Pt, PosEqualEps);
						};

						if (Intr.Count >= 1 && IsValidIntersect(Intr.Point1, Intr.T1))
							ValidPoints.Add(Intr.Point1);
						if (Intr.Count >= 2 && IsValidIntersect(Intr.Point2, Intr.T2))
							ValidPoints.Add(Intr.Point2);

						if (ValidPoints.Num() == 1)
						{
							Result.Type = ESegIntersectType::OnePoint;
							Result.Point1 = ValidPoints[0];
						}
						else if (ValidPoints.Num() == 2)
						{
							Result.Type = ESegIntersectType::TwoPoints;
							// Order by distance from U1
							if (Math::DistanceSquared(U1Pos, ValidPoints[0]) >
								Math::DistanceSquared(U1Pos, ValidPoints[1]))
							{
								Swap(ValidPoints[0], ValidPoints[1]);
							}
							Result.Point1 = ValidPoints[0];
							Result.Point2 = ValidPoints[1];
						}
					}
				}
				else if (!bV1IsLine && bU1IsLine)
				{
					// Arc-Line intersection (swap and call recursively)
					FSegIntersectResult Swapped = ComputeSegmentIntersection(U1, U2, V1, V2, PosEqualEps);
					Result = Swapped;
				}
				else
				{
					// Arc-Arc intersection
					const Math::FArcGeometry Arc1 = Math::ComputeArcRadiusAndCenter(V1, V2);
					const Math::FArcGeometry Arc2 = Math::ComputeArcRadiusAndCenter(U1, U2);

					if (Arc1.bValid && Arc2.bValid)
					{
						// Check if arcs are concentric and overlapping
						if (Arc1.Center.Equals(Arc2.Center, PosEqualEps) &&
							FMath::Abs(Arc1.Radius - Arc2.Radius) < PosEqualEps)
						{
							// Potentially overlapping arcs - check sweep overlap
							// This is simplified; full implementation would compute arc overlap region
							Result.Type = ESegIntersectType::OverlappingArcs;
							Result.Point1 = V1Pos;
							Result.Point2 = V2Pos;
						}
						else
						{
							const Math::FCircleCircleIntersect Intr = Math::CircleCircleIntersection(
								Arc1.Center, Arc1.Radius, Arc2.Center, Arc2.Radius, PosEqualEps);

							TArray<FVector2D> ValidPoints;

							auto IsValid = [&](const FVector2D& Pt) -> bool
							{
								return Math::PointWithinArcSweep(Arc1.Center, V1Pos, V2Pos, V1.Bulge < 0.0, Pt, PosEqualEps)
									&& Math::PointWithinArcSweep(Arc2.Center, U1Pos, U2Pos, U1.Bulge < 0.0, Pt, PosEqualEps);
							};

							if (Intr.Count >= 1 && IsValid(Intr.Point1))
								ValidPoints.Add(Intr.Point1);
							if (Intr.Count >= 2 && IsValid(Intr.Point2))
								ValidPoints.Add(Intr.Point2);

							if (ValidPoints.Num() == 1)
							{
								Result.Type = ESegIntersectType::OnePoint;
								Result.Point1 = ValidPoints[0];
							}
							else if (ValidPoints.Num() == 2)
							{
								Result.Type = ESegIntersectType::TwoPoints;
								if (Math::DistanceSquared(U1Pos, ValidPoints[0]) >
									Math::DistanceSquared(U1Pos, ValidPoints[1]))
								{
									Swap(ValidPoints[0], ValidPoints[1]);
								}
								Result.Point1 = ValidPoints[0];
								Result.Point2 = ValidPoints[1];
							}
						}
					}
				}

				return Result;
			}

			//=============================================================================
			// Find All Intersections
			//=============================================================================

			FIntersectsCollection FindIntersects(
				const FPolyline& Pline1,
				const FPolyline& Pline2,
				double PosEqualEps)
			{
				FIntersectsCollection Result;

				if (Pline1.VertexCount() < 2 || Pline2.VertexCount() < 2)
				{
					return Result;
				}

				const int32 VC1 = Pline1.VertexCount();
				const int32 VC2 = Pline2.VertexCount();
				const int32 Open1LastIdx = Pline1.IsClosed() ? -1 : VC1 - 2;
				const int32 Open2LastIdx = Pline2.IsClosed() ? -1 : VC2 - 2;

				TSet<int32> PossibleDuplicates1;
				TSet<int32> PossibleDuplicates2;

				// Iterate all segment pairs
				Pline1.ForEachSegment([&](const FVertex& V1, const FVertex& V2)
				{
					const int32 I1 = &V1 - &Pline1.GetVertex(0);

					Pline2.ForEachSegment([&](const FVertex& U1, const FVertex& U2)
					{
						const int32 I2 = &U1 - &Pline2.GetVertex(0);

						const FSegIntersectResult Intr = ComputeSegmentIntersection(V1, V2, U1, U2, PosEqualEps);

						auto ShouldSkipAtEnd = [&](const FVector2D& Point) -> bool
						{
							// Skip intersect at end of segment (will be found at start of next segment)
							const bool bAtV2 = V2.GetPosition().Equals(Point, PosEqualEps);
							const bool bAtU2 = U2.GetPosition().Equals(Point, PosEqualEps);

							if (bAtV2 && (Pline1.IsClosed() || I1 != Open1LastIdx))
								return true;
							if (bAtU2 && (Pline2.IsClosed() || I2 != Open2LastIdx))
								return true;
							return false;
						};

						switch (Intr.Type)
						{
						case ESegIntersectType::OnePoint:
						case ESegIntersectType::Tangent:
							if (!ShouldSkipAtEnd(Intr.Point1))
							{
								Result.BasicIntersects.Add(FBasicIntersect(I1, I2, Intr.Point1));
							}
							break;

						case ESegIntersectType::TwoPoints:
							if (!ShouldSkipAtEnd(Intr.Point1))
							{
								Result.BasicIntersects.Add(FBasicIntersect(I1, I2, Intr.Point1));
							}
							if (!ShouldSkipAtEnd(Intr.Point2))
							{
								Result.BasicIntersects.Add(FBasicIntersect(I1, I2, Intr.Point2));
							}
							break;

						case ESegIntersectType::OverlappingLines:
						case ESegIntersectType::OverlappingArcs:
							Result.OverlappingIntersects.Add(FOverlappingIntersect(I1, I2, Intr.Point1, Intr.Point2));

							// Track possible duplicates
							if (V2.GetPosition().Equals(Intr.Point1, PosEqualEps) ||
								V2.GetPosition().Equals(Intr.Point2, PosEqualEps))
							{
								PossibleDuplicates1.Add(Pline1.NextWrappingIndex(I1));
							}
							if (U2.GetPosition().Equals(Intr.Point1, PosEqualEps) ||
								U2.GetPosition().Equals(Intr.Point2, PosEqualEps))
							{
								PossibleDuplicates2.Add(Pline2.NextWrappingIndex(I2));
							}
							break;

						default:
							break;
						}
					});
				});

				// Remove duplicate basic intersects caused by overlapping
				if (PossibleDuplicates1.Num() > 0 || PossibleDuplicates2.Num() > 0)
				{
					TArray<FBasicIntersect> Filtered;
					Filtered.Reserve(Result.BasicIntersects.Num());

					for (const FBasicIntersect& Intr : Result.BasicIntersects)
					{
						bool bSkip = false;

						if (PossibleDuplicates1.Contains(Intr.StartIndex1))
						{
							if (Intr.Point.Equals(Pline1.GetVertex(Intr.StartIndex1).GetPosition(), PosEqualEps))
							{
								bSkip = true;
							}
						}
						if (!bSkip && PossibleDuplicates2.Contains(Intr.StartIndex2))
						{
							if (Intr.Point.Equals(Pline2.GetVertex(Intr.StartIndex2).GetPosition(), PosEqualEps))
							{
								bSkip = true;
							}
						}

						if (!bSkip)
						{
							Filtered.Add(Intr);
						}
					}

					Result.BasicIntersects = MoveTemp(Filtered);
				}

				return Result;
			}

			//=============================================================================
			// Segment Splitting
			//=============================================================================

			struct FSplitResult
			{
				FVertex UpdatedStart;
				FVertex SplitVertex;
			};

			FSplitResult SplitSegmentAtPoint(
				const FVertex& V1,
				const FVertex& V2,
				const FVector2D& SplitPoint,
				double PosEqualEps)
			{
				FSplitResult Result;

				if (V1.IsLine(PosEqualEps))
				{
					// Line segment
					Result.UpdatedStart = V1;
					Result.SplitVertex = FVertex(SplitPoint, 0.0);
					return Result;
				}

				const FVector2D V1Pos = V1.GetPosition();
				const FVector2D V2Pos = V2.GetPosition();

				// Check degenerate cases
				if (V1Pos.Equals(V2Pos, PosEqualEps) || V1Pos.Equals(SplitPoint, PosEqualEps))
				{
					Result.UpdatedStart = FVertex(SplitPoint, 0.0);
					Result.SplitVertex = FVertex(SplitPoint, V1.Bulge);
					return Result;
				}

				if (V2Pos.Equals(SplitPoint, PosEqualEps))
				{
					Result.UpdatedStart = V1;
					Result.SplitVertex = FVertex(V2Pos, 0.0);
					return Result;
				}

				// Arc segment - compute new bulges
				const Math::FArcGeometry Arc = Math::ComputeArcRadiusAndCenter(V1, V2);
				if (!Arc.bValid)
				{
					Result.UpdatedStart = V1;
					Result.SplitVertex = FVertex(SplitPoint, 0.0);
					return Result;
				}

				const double PointAngle = Math::Angle(Arc.Center, SplitPoint);
				const double StartAngle = Math::Angle(Arc.Center, V1Pos);
				const double EndAngle = Math::Angle(Arc.Center, V2Pos);

				const double Theta1 = Math::DeltaAngleSigned(StartAngle, PointAngle, V1.Bulge < 0.0);
				const double Bulge1 = Math::BulgeFromAngle(Theta1);

				const double Theta2 = Math::DeltaAngleSigned(PointAngle, EndAngle, V1.Bulge < 0.0);
				const double Bulge2 = Math::BulgeFromAngle(Theta2);

				Result.UpdatedStart = FVertex(V1Pos, Bulge1);
				Result.SplitVertex = FVertex(SplitPoint, Bulge2);

				return Result;
			}

			//=============================================================================
			// Slice Point for tracking intersects
			//=============================================================================

			struct FSlicePoint
			{
				FVector2D Position;
				bool bIsStartOfOverlappingSlice;

				FSlicePoint()
					: Position(FVector2D::ZeroVector), bIsStartOfOverlappingSlice(false)
				{
				}

				FSlicePoint(const FVector2D& Pos, bool bOverlapStart)
					: Position(Pos), bIsStartOfOverlappingSlice(bOverlapStart)
				{
				}
			};

			//=============================================================================
			// Process Polylines for Boolean Operations
			//=============================================================================

			FProcessedBoolean ProcessForBoolean(
				const FPolyline& Pline1,
				const FPolyline& Pline2,
				double PosEqualEps)
			{
				FProcessedBoolean Result;

				// Find all intersections
				FIntersectsCollection Intrs = FindIntersects(Pline1, Pline2, PosEqualEps);
				Result.Intersects = MoveTemp(Intrs.BasicIntersects);

				// Convert overlapping intersects to overlapping slices
				// (Simplified - full implementation would join adjacent overlapping segments)
				for (const FOverlappingIntersect& OvIntr : Intrs.OverlappingIntersects)
				{
					FOverlappingSlice Slice;
					Slice.StartIndexes = TPair<int32, int32>(OvIntr.StartIndex1, OvIntr.StartIndex2);
					Slice.EndIndexes = Slice.StartIndexes;
					Slice.UpdatedStart = FVertex(OvIntr.Point1, 0.0);
					Slice.UpdatedEndBulge = 0.0;
					Slice.EndPoint = OvIntr.Point2;
					Slice.bIsLoop = false;
					Slice.bOpposingDirections = false;
					Result.OverlappingSlices.Add(Slice);
				}

				Result.Pline1Orientation = Pline1.Orientation();
				Result.Pline2Orientation = Pline2.Orientation();

				return Result;
			}

			//=============================================================================
			// Slice Polyline at Intersects
			//=============================================================================

			void SliceAtIntersects(
				const FPolyline& Pline,
				const FProcessedBoolean& BooleanInfo,
				bool bUseSecondIndex,
				TFunctionRef<bool(const FVector2D&)> PointOnSlicePredicate,
				TArray<FBooleanSlice>& OutSlices,
				double PosEqualEps)
			{
				// Build lookup of intersects by segment index
				TMap<int32, TArray<FSlicePoint>> IntersectsLookup;

				if (bUseSecondIndex)
				{
					for (const FBasicIntersect& Intr : BooleanInfo.Intersects)
					{
						IntersectsLookup.FindOrAdd(Intr.StartIndex2).Add(FSlicePoint(Intr.Point, false));
					}

					for (const FOverlappingSlice& OvSlice : BooleanInfo.OverlappingSlices)
					{
						FVector2D SP = OvSlice.UpdatedStart.GetPosition();
						FVector2D EP = OvSlice.EndPoint;
						int32 SPIdx = OvSlice.StartIndexes.Value;
						int32 EPIdx = OvSlice.EndIndexes.Value;

						const int32 SPIdxNext = Pline.NextWrappingIndex(SPIdx);
						if (SP.Equals(Pline.GetVertex(SPIdxNext).GetPosition(), PosEqualEps))
							SPIdx = SPIdxNext;
						const int32 EPIdxNext = Pline.NextWrappingIndex(EPIdx);
						if (EP.Equals(Pline.GetVertex(EPIdxNext).GetPosition(), PosEqualEps))
							EPIdx = EPIdxNext;

						IntersectsLookup.FindOrAdd(SPIdx).Add(FSlicePoint(SP, true));
						IntersectsLookup.FindOrAdd(EPIdx).Add(FSlicePoint(EP, false));
					}
				}
				else
				{
					for (const FBasicIntersect& Intr : BooleanInfo.Intersects)
					{
						IntersectsLookup.FindOrAdd(Intr.StartIndex1).Add(FSlicePoint(Intr.Point, false));
					}

					for (const FOverlappingSlice& OvSlice : BooleanInfo.OverlappingSlices)
					{
						FVector2D SP = OvSlice.UpdatedStart.GetPosition();
						FVector2D EP = OvSlice.EndPoint;
						int32 SPIdx = OvSlice.StartIndexes.Key;
						int32 EPIdx = OvSlice.EndIndexes.Key;

						const int32 SPIdxNext = Pline.NextWrappingIndex(SPIdx);
						if (SP.Equals(Pline.GetVertex(SPIdxNext).GetPosition(), PosEqualEps))
							SPIdx = SPIdxNext;
						const int32 EPIdxNext = Pline.NextWrappingIndex(EPIdx);
						if (EP.Equals(Pline.GetVertex(EPIdxNext).GetPosition(), PosEqualEps))
							EPIdx = EPIdxNext;

						const bool bSPIsSliceStart = !OvSlice.bOpposingDirections;
						IntersectsLookup.FindOrAdd(SPIdx).Add(FSlicePoint(SP, bSPIsSliceStart));
						IntersectsLookup.FindOrAdd(EPIdx).Add(FSlicePoint(EP, !bSPIsSliceStart));
					}
				}

				// Sort intersects by distance from segment start
				for (auto& Pair : IntersectsLookup)
				{
					const FVector2D StartPos = Pline.GetVertex(Pair.Key).GetPosition();
					Pair.Value.Sort([&StartPos](const FSlicePoint& A, const FSlicePoint& B)
					{
						return Math::DistanceSquared(A.Position, StartPos) <
							Math::DistanceSquared(B.Position, StartPos);
					});
				}

				// Create slices from intersects
				for (const auto& Pair : IntersectsLookup)
				{
					const int32 StartIndex = Pair.Key;
					const TArray<FSlicePoint>& IntrList = Pair.Value;
					const int32 NextIndex = Pline.NextWrappingIndex(StartIndex);
					const FVertex StartVertex = Pline.GetVertex(StartIndex);
					const FVertex EndVertex = Pline.GetVertex(NextIndex);

					// Handle multiple intersects on same segment
					if (IntrList.Num() > 1)
					{
						FSplitResult FirstSplit = SplitSegmentAtPoint(StartVertex, EndVertex, IntrList[0].Position, PosEqualEps);
						FVertex PrevVertex = FirstSplit.SplitVertex;

						for (int32 i = 1; i < IntrList.Num(); ++i)
						{
							FSplitResult Split = SplitSegmentAtPoint(PrevVertex, EndVertex, IntrList[i].Position, PosEqualEps);
							PrevVertex = Split.SplitVertex;

							if (IntrList[i - 1].bIsStartOfOverlappingSlice)
								continue;

							if (Split.UpdatedStart.GetPosition().Equals(Split.SplitVertex.GetPosition(), PosEqualEps))
								continue;

							const FVector2D Midpoint = Math::SegmentMidpoint(Split.UpdatedStart, Split.SplitVertex);
							if (!PointOnSlicePredicate(Midpoint))
								continue;

							FBooleanSlice Slice;
							Slice.StartIndex = StartIndex;
							Slice.EndIndexOffset = 0;
							Slice.UpdatedStart = Split.UpdatedStart;
							Slice.UpdatedEndBulge = Split.SplitVertex.Bulge;
							Slice.EndPoint = Split.SplitVertex.GetPosition();
							Slice.bInvertedDirection = false;
							Slice.bSourceIsPline1 = !bUseSecondIndex;
							Slice.bIsOverlapping = false;
							OutSlices.Add(Slice);
						}
					}

					// Handle slice from last intersect to next intersect
					const FSlicePoint& LastIntr = IntrList.Last();

					if (LastIntr.bIsStartOfOverlappingSlice)
						continue;

					FSplitResult SliceStartSplit = SplitSegmentAtPoint(StartVertex, EndVertex, LastIntr.Position, PosEqualEps);
					FVertex SliceStartVertex = SliceStartSplit.SplitVertex;

					int32 Index = NextIndex;
					int32 LoopCount = 0;
					const int32 MaxLoopCount = Pline.VertexCount();

					while (LoopCount <= MaxLoopCount)
					{
						++LoopCount;

						if (const TArray<FSlicePoint>* NextIntrList = IntersectsLookup.Find(Index))
						{
							const FVector2D IntersectPoint = (*NextIntrList)[0].Position;

							int32 VertexOffset = 0;
							int32 TempIdx = StartIndex;
							while (TempIdx != Index)
							{
								TempIdx = Pline.NextWrappingIndex(TempIdx);
								++VertexOffset;
							}

							FBooleanSlice Slice;
							Slice.StartIndex = StartIndex;
							Slice.EndIndexOffset = VertexOffset;
							Slice.UpdatedStart = SliceStartVertex;
							Slice.EndPoint = IntersectPoint;
							Slice.bInvertedDirection = false;
							Slice.bSourceIsPline1 = !bUseSecondIndex;
							Slice.bIsOverlapping = false;

							const FVertex SegStart = Pline.GetVertex(Index);
							const FVertex SegEnd = Pline.GetVertex(Pline.NextWrappingIndex(Index));
							FSplitResult EndSplit = SplitSegmentAtPoint(SegStart, SegEnd, IntersectPoint, PosEqualEps);
							Slice.UpdatedEndBulge = EndSplit.UpdatedStart.Bulge;

							const FVertex NextV = Pline.GetVertex(Pline.NextWrappingIndex(Slice.StartIndex));
							const FVector2D Midpoint = Math::SegmentMidpoint(Slice.UpdatedStart, NextV);

							if (PointOnSlicePredicate(Midpoint))
							{
								OutSlices.Add(Slice);
							}

							break;
						}

						Index = Pline.NextWrappingIndex(Index);
					}
				}
			}

			//=============================================================================
			// Prune Slices Based on Boolean Operation
			//=============================================================================

			FPrunedSlices PruneSlices(
				const FPolyline& Pline1,
				const FPolyline& Pline2,
				const FProcessedBoolean& BooleanInfo,
				EPCGExCCBooleanOp Operation,
				double PosEqualEps)
			{
				FPrunedSlices Result;

				auto PointInPline1 = [&Pline1](const FVector2D& Pt) -> bool
				{
					return Pline1.WindingNumber(Pt) != 0;
				};

				auto PointInPline2 = [&Pline2](const FVector2D& Pt) -> bool
				{
					return Pline2.WindingNumber(Pt) != 0;
				};

				// Slice pline1
				switch (Operation)
				{
				case EPCGExCCBooleanOp::Union:
					SliceAtIntersects(Pline1, BooleanInfo, false,
					                  [&](const FVector2D& Pt) { return !PointInPline2(Pt); },
					                  Result.Slices, PosEqualEps);
					break;
				case EPCGExCCBooleanOp::Intersection:
					SliceAtIntersects(Pline1, BooleanInfo, false,
					                  PointInPline2,
					                  Result.Slices, PosEqualEps);
					break;
				case EPCGExCCBooleanOp::Difference:
				case EPCGExCCBooleanOp::Xor:
					SliceAtIntersects(Pline1, BooleanInfo, false,
					                  [&](const FVector2D& Pt) { return !PointInPline2(Pt); },
					                  Result.Slices, PosEqualEps);
					break;
				}

				Result.StartOfPline2Slices = Result.Slices.Num();

				// Slice pline2
				switch (Operation)
				{
				case EPCGExCCBooleanOp::Union:
				case EPCGExCCBooleanOp::Xor:
					SliceAtIntersects(Pline2, BooleanInfo, true,
					                  [&](const FVector2D& Pt) { return !PointInPline1(Pt); },
					                  Result.Slices, PosEqualEps);
					break;
				case EPCGExCCBooleanOp::Intersection:
				case EPCGExCCBooleanOp::Difference:
					SliceAtIntersects(Pline2, BooleanInfo, true,
					                  PointInPline1,
					                  Result.Slices, PosEqualEps);
					break;
				}

				Result.StartOfPline1OverlappingSlices = Result.Slices.Num();

				// Add overlapping slices from pline1
				for (const FOverlappingSlice& OvSlice : BooleanInfo.OverlappingSlices)
				{
					FBooleanSlice Slice;
					Slice.StartIndex = OvSlice.StartIndexes.Value;
					Slice.UpdatedStart = OvSlice.UpdatedStart;
					Slice.UpdatedEndBulge = OvSlice.UpdatedEndBulge;
					Slice.EndPoint = OvSlice.EndPoint;
					Slice.bInvertedDirection = OvSlice.bOpposingDirections;
					Slice.bSourceIsPline1 = false;
					Slice.bIsOverlapping = true;
					Result.Slices.Add(Slice);
				}

				Result.StartOfPline2OverlappingSlices = Result.Slices.Num();

				// Add overlapping slices from pline2
				for (const FOverlappingSlice& OvSlice : BooleanInfo.OverlappingSlices)
				{
					FBooleanSlice Slice;
					Slice.StartIndex = OvSlice.StartIndexes.Value;
					Slice.UpdatedStart = OvSlice.UpdatedStart;
					Slice.UpdatedEndBulge = OvSlice.UpdatedEndBulge;
					Slice.EndPoint = OvSlice.EndPoint;
					Slice.bInvertedDirection = false;
					Slice.bSourceIsPline1 = false;
					Slice.bIsOverlapping = true;
					Result.Slices.Add(Slice);
				}

				// Invert pline1 slice directions if needed
				const bool bSetOpposingDirection = (Operation == EPCGExCCBooleanOp::Difference ||
					Operation == EPCGExCCBooleanOp::Xor);
				if (bSetOpposingDirection != BooleanInfo.HasOpposingDirections())
				{
					for (int32 i = 0; i < Result.StartOfPline2Slices; ++i)
					{
						Result.Slices[i].bInvertedDirection = true;
					}
				}

				return Result;
			}

			//=============================================================================
			// Stitch Selector Interface
			//=============================================================================

			/** Select next slice to stitch for OR/AND operations */
			int32 SelectOrAndStitch(
				int32 CurrentSliceIdx,
				const TArray<int32>& AvailableIdx,
				const FPrunedSlices& PrunedSlices)
			{
				const bool bIsPline1Idx = CurrentSliceIdx < PrunedSlices.StartOfPline2Slices
					|| (CurrentSliceIdx >= PrunedSlices.StartOfPline1OverlappingSlices
						&& CurrentSliceIdx < PrunedSlices.StartOfPline2OverlappingSlices);

				if (bIsPline1Idx)
				{
					// Try non-overlapping pline2
					for (int32 Idx : AvailableIdx)
					{
						if (Idx >= PrunedSlices.StartOfPline2Slices && Idx < PrunedSlices.StartOfPline1OverlappingSlices)
							return Idx;
					}
					// Try non-overlapping pline1
					for (int32 Idx : AvailableIdx)
					{
						if (Idx < PrunedSlices.StartOfPline2Slices)
							return Idx;
					}
				}
				else
				{
					// Try non-overlapping pline1
					for (int32 Idx : AvailableIdx)
					{
						if (Idx < PrunedSlices.StartOfPline2Slices)
							return Idx;
					}
					// Try non-overlapping pline2
					for (int32 Idx : AvailableIdx)
					{
						if (Idx >= PrunedSlices.StartOfPline2Slices && Idx < PrunedSlices.StartOfPline1OverlappingSlices)
							return Idx;
					}
				}

				return AvailableIdx.Num() > 0 ? AvailableIdx[0] : INDEX_NONE;
			}

			/** Select next slice to stitch for NOT/XOR operations */
			int32 SelectNotXorStitch(
				int32 CurrentSliceIdx,
				const TArray<int32>& AvailableIdx,
				const FPrunedSlices& PrunedSlices)
			{
				auto IdxForPline1 = [&]() -> int32
				{
					for (int32 Idx : AvailableIdx)
					{
						if (Idx < PrunedSlices.StartOfPline2Slices)
							return Idx;
					}
					return INDEX_NONE;
				};

				auto IdxForPline2 = [&]() -> int32
				{
					for (int32 Idx : AvailableIdx)
					{
						if (Idx >= PrunedSlices.StartOfPline2Slices && Idx < PrunedSlices.StartOfPline1OverlappingSlices)
							return Idx;
					}
					return INDEX_NONE;
				};

				if (CurrentSliceIdx >= PrunedSlices.StartOfPline1OverlappingSlices)
				{
					// Current is overlapping
					if (CurrentSliceIdx < PrunedSlices.StartOfPline2OverlappingSlices)
					{
						// From pline1 - try pline2 then pline1
						int32 Result = IdxForPline2();
						if (Result == INDEX_NONE) Result = IdxForPline1();
						return Result;
					}
					else
					{
						// From pline2 - try pline1 then pline2
						int32 Result = IdxForPline1();
						if (Result == INDEX_NONE) Result = IdxForPline2();
						return Result;
					}
				}

				// Current is not overlapping
				if (CurrentSliceIdx < PrunedSlices.StartOfPline2Slices)
				{
					// From pline1 - prefer pline2
					int32 Result = IdxForPline2();
					if (Result == INDEX_NONE && AvailableIdx.Num() > 0)
						Result = AvailableIdx[0];
					return Result;
				}
				else
				{
					// From pline2 - prefer pline1
					int32 Result = IdxForPline1();
					if (Result == INDEX_NONE && AvailableIdx.Num() > 0)
						Result = AvailableIdx[0];
					return Result;
				}
			}

			//=============================================================================
			// Build Polyline from Slice
			//=============================================================================

			FPolyline BuildPolylineFromSlice(
				const FBooleanSlice& Slice,
				const FPolyline& Pline1,
				const FPolyline& Pline2,
				double PosEqualEps)
			{
				const FPolyline& Source = Slice.bSourceIsPline1 ? Pline1 : Pline2;
				FPolyline Result(false);

				if (Slice.bInvertedDirection)
				{
					// Build in reverse
					Result.AddVertex(FVertex(Slice.EndPoint, 0.0));

					for (int32 i = Slice.EndIndexOffset; i >= 0; --i)
					{
						int32 Idx = Slice.StartIndex;
						for (int32 j = 0; j < i; ++j)
							Idx = Source.NextWrappingIndex(Idx);

						const FVertex& V = Source.GetVertex(Idx);
						const int32 PrevIdx = Source.PrevWrappingIndex(Idx);
						const double NegatedBulge = -Source.GetVertex(PrevIdx).Bulge;
						Result.AddVertex(FVertex(V.GetPosition(), NegatedBulge));
					}

					// Update start vertex bulge
					if (Result.VertexCount() > 0)
					{
						FVertex& Last = const_cast<FVertex&>(Result.GetVertex(Result.VertexCount() - 1));
						Last = FVertex(Last.GetPosition(), -Slice.UpdatedStart.Bulge);
					}
				}
				else
				{
					Result.AddVertex(Slice.UpdatedStart);

					for (int32 i = 1; i <= Slice.EndIndexOffset; ++i)
					{
						int32 Idx = Slice.StartIndex;
						for (int32 j = 0; j < i; ++j)
							Idx = Source.NextWrappingIndex(Idx);
						Result.AddVertex(Source.GetVertex(Idx));
					}

					// Set end bulge
					if (Result.VertexCount() > 0)
					{
						FVertex& Last = const_cast<FVertex&>(Result.GetVertex(Result.VertexCount() - 1));
						Last = FVertex(Last.GetPosition(), Slice.UpdatedEndBulge);
					}

					Result.AddVertex(FVertex(Slice.EndPoint, 0.0));
				}

				return Result;
			}

			void ExtendPolylineFromSlice(
				FPolyline& Target,
				const FBooleanSlice& Slice,
				const FPolyline& Pline1,
				const FPolyline& Pline2,
				double PosEqualEps)
			{
				FPolyline SlicePline = BuildPolylineFromSlice(Slice, Pline1, Pline2, PosEqualEps);

				// Remove duplicate start point
				if (Target.VertexCount() > 0 && SlicePline.VertexCount() > 0)
				{
					if (Target.GetVertex(Target.VertexCount() - 1).GetPosition().Equals(
						SlicePline.GetVertex(0).GetPosition(), PosEqualEps))
					{
						Target.RemoveLastVertex();
					}
				}

				// Append vertices
				for (int32 i = 0; i < SlicePline.VertexCount(); ++i)
				{
					Target.AddVertex(SlicePline.GetVertex(i));
				}
			}

			//=============================================================================
			// Stitch Slices Into Closed Polylines
			//=============================================================================

			TArray<FPolyline> StitchSlicesIntoClosedPolylines(
				const FPrunedSlices& PrunedSlices,
				const FPolyline& Pline1,
				const FPolyline& Pline2,
				EPCGExCCBooleanOp Operation,
				double PosEqualEps,
				double CollapsedAreaEps)
			{
				TArray<FPolyline> Results;

				if (PrunedSlices.Slices.Num() == 0)
					return Results;

				const bool bUseNotXorSelector = (Operation == EPCGExCCBooleanOp::Difference ||
					Operation == EPCGExCCBooleanOp::Xor);

				TArray<bool> VisitedSliceIdx;
				VisitedSliceIdx.SetNumZeroed(PrunedSlices.Slices.Num());

				for (int32 i = 0; i < PrunedSlices.Slices.Num(); ++i)
				{
					if (VisitedSliceIdx[i])
						continue;

					VisitedSliceIdx[i] = true;

					const FBooleanSlice& StartSlice = PrunedSlices.Slices[i];
					FPolyline CurrentPline = BuildPolylineFromSlice(StartSlice, Pline1, Pline2, PosEqualEps);

					const int32 BeginningSliceIdx = i;
					int32 CurrentSliceIdx = i;
					int32 LoopCount = 0;
					const int32 MaxLoopCount = PrunedSlices.Slices.Num();

					while (LoopCount <= MaxLoopCount)
					{
						++LoopCount;

						// Find slices whose start connects to current end
						const FVector2D EndPoint = CurrentPline.GetVertex(CurrentPline.VertexCount() - 1).GetPosition();
						TArray<int32> QueryResults;

						for (int32 j = 0; j < PrunedSlices.Slices.Num(); ++j)
						{
							if (j == BeginningSliceIdx || !VisitedSliceIdx[j])
							{
								const FBooleanSlice& Slice = PrunedSlices.Slices[j];
								const FVector2D SliceStart = Slice.bInvertedDirection ? Slice.EndPoint : Slice.GetStartPoint();

								if (EndPoint.Equals(SliceStart, PosEqualEps))
								{
									QueryResults.Add(j);
								}
							}
						}

						if (QueryResults.Num() == 0)
							break;

						int32 ConnectedSliceIdx = bUseNotXorSelector
							                          ? SelectNotXorStitch(CurrentSliceIdx, QueryResults, PrunedSlices)
							                          : SelectOrAndStitch(CurrentSliceIdx, QueryResults, PrunedSlices);

						if (ConnectedSliceIdx == INDEX_NONE)
							break;

						if (ConnectedSliceIdx == BeginningSliceIdx)
						{
							// Connected back to beginning - close and add
							if (CurrentPline.VertexCount() >= 3)
							{
								// Check if start connects to end
								if (CurrentPline.GetVertex(0).GetPosition().Equals(
									CurrentPline.GetVertex(CurrentPline.VertexCount() - 1).GetPosition(), PosEqualEps))
								{
									CurrentPline.RemoveLastVertex();
								}

								// Close polyline
								FPolyline ClosedPline(true);
								for (int32 v = 0; v < CurrentPline.VertexCount(); ++v)
								{
									ClosedPline.AddVertex(CurrentPline.GetVertex(v));
								}

								// Check area threshold
								if (CollapsedAreaEps <= 0.0 || FMath::Abs(ClosedPline.Area()) >= CollapsedAreaEps)
								{
									Results.Add(MoveTemp(ClosedPline));
								}
							}
							break;
						}

						// Stitch connected slice
						CurrentPline.RemoveLastVertex();
						ExtendPolylineFromSlice(CurrentPline, PrunedSlices.Slices[ConnectedSliceIdx], Pline1, Pline2, PosEqualEps);
						VisitedSliceIdx[ConnectedSliceIdx] = true;
						CurrentSliceIdx = ConnectedSliceIdx;
					}
				}

				return Results;
			}
		}

		//=============================================================================
		// Main Boolean Operation
		//=============================================================================

		FBooleanResult PerformBoolean(
			const FPolyline& Pline1,
			const FPolyline& Pline2,
			EPCGExCCBooleanOp Operation,
			const FContourBooleanOptions& Options)
		{
			FBooleanResult Result;

			// Validate input
			if (Pline1.VertexCount() < 2 || !Pline1.IsClosed() ||
				Pline2.VertexCount() < 2 || !Pline2.IsClosed())
			{
				Result.ResultInfo = EBooleanResultInfo::InvalidInput;
				return Result;
			}

			const double PosEqualEps = Options.PositionEqualEpsilon;
			const double CollapsedAreaEps = 1e-10;

			// Process polylines for boolean
			Internal::FProcessedBoolean BooleanInfo = Internal::ProcessForBoolean(Pline1, Pline2, PosEqualEps);

			// Helper functions for containment checks
			auto IsPline1InPline2 = [&]() -> bool
			{
				return Pline2.WindingNumber(Pline1.GetVertex(0).GetPosition()) != 0;
			};

			auto IsPline2InPline1 = [&]() -> bool
			{
				return Pline1.WindingNumber(Pline2.GetVertex(0).GetPosition()) != 0;
			};

			// Handle each operation
			switch (Operation)
			{
			case EPCGExCCBooleanOp::Union:
				if (BooleanInfo.IsCompletelyOverlapping())
				{
					Result.PositivePolylines.Add(Pline2);
					Result.ResultInfo = EBooleanResultInfo::Overlapping;
				}
				else if (!BooleanInfo.HasAnyIntersects())
				{
					if (IsPline1InPline2())
					{
						Result.PositivePolylines.Add(Pline2);
						Result.ResultInfo = EBooleanResultInfo::Pline1InsidePline2;
					}
					else if (IsPline2InPline1())
					{
						Result.PositivePolylines.Add(Pline1);
						Result.ResultInfo = EBooleanResultInfo::Pline2InsidePline1;
					}
					else
					{
						Result.PositivePolylines.Add(Pline1);
						Result.PositivePolylines.Add(Pline2);
						Result.ResultInfo = EBooleanResultInfo::Disjoint;
					}
				}
				else
				{
					Internal::FPrunedSlices Pruned = Internal::PruneSlices(Pline1, Pline2, BooleanInfo, Operation, PosEqualEps);
					TArray<FPolyline> Remaining = StitchSlicesIntoClosedPolylines(
						Pruned, Pline1, Pline2, Operation, PosEqualEps, CollapsedAreaEps);

					for (FPolyline& Pline : Remaining)
					{
						const EPCGExCCOrientation Orientation = Pline.Orientation();
						if (Orientation != BooleanInfo.Pline2Orientation)
						{
							Result.NegativePolylines.Add(MoveTemp(Pline));
						}
						else
						{
							Result.PositivePolylines.Add(MoveTemp(Pline));
						}
					}
					Result.ResultInfo = EBooleanResultInfo::Intersected;
				}
				break;

			case EPCGExCCBooleanOp::Intersection:
				if (BooleanInfo.IsCompletelyOverlapping())
				{
					Result.PositivePolylines.Add(Pline2);
					Result.ResultInfo = EBooleanResultInfo::Overlapping;
				}
				else if (!BooleanInfo.HasAnyIntersects())
				{
					if (IsPline1InPline2())
					{
						Result.PositivePolylines.Add(Pline1);
						Result.ResultInfo = EBooleanResultInfo::Pline1InsidePline2;
					}
					else if (IsPline2InPline1())
					{
						Result.PositivePolylines.Add(Pline2);
						Result.ResultInfo = EBooleanResultInfo::Pline2InsidePline1;
					}
					else
					{
						Result.ResultInfo = EBooleanResultInfo::Disjoint;
					}
				}
				else
				{
					Internal::FPrunedSlices Pruned = Internal::PruneSlices(Pline1, Pline2, BooleanInfo, Operation, PosEqualEps);
					Result.PositivePolylines = StitchSlicesIntoClosedPolylines(
						Pruned, Pline1, Pline2, Operation, PosEqualEps, CollapsedAreaEps);
					Result.ResultInfo = EBooleanResultInfo::Intersected;
				}
				break;

			case EPCGExCCBooleanOp::Difference:
				if (BooleanInfo.IsCompletelyOverlapping())
				{
					Result.ResultInfo = EBooleanResultInfo::Overlapping;
				}
				else if (!BooleanInfo.HasAnyIntersects())
				{
					if (IsPline1InPline2())
					{
						Result.ResultInfo = EBooleanResultInfo::Pline1InsidePline2;
					}
					else if (IsPline2InPline1())
					{
						Result.PositivePolylines.Add(Pline1);
						Result.NegativePolylines.Add(Pline2);
						Result.ResultInfo = EBooleanResultInfo::Pline2InsidePline1;
					}
					else
					{
						Result.PositivePolylines.Add(Pline1);
						Result.ResultInfo = EBooleanResultInfo::Disjoint;
					}
				}
				else
				{
					Internal::FPrunedSlices Pruned = Internal::PruneSlices(Pline1, Pline2, BooleanInfo, Operation, PosEqualEps);
					Result.PositivePolylines = StitchSlicesIntoClosedPolylines(
						Pruned, Pline1, Pline2, Operation, PosEqualEps, CollapsedAreaEps);
					Result.ResultInfo = EBooleanResultInfo::Intersected;
				}
				break;

			case EPCGExCCBooleanOp::Xor:
				if (BooleanInfo.IsCompletelyOverlapping())
				{
					Result.ResultInfo = EBooleanResultInfo::Overlapping;
				}
				else if (!BooleanInfo.HasAnyIntersects())
				{
					if (IsPline1InPline2())
					{
						Result.PositivePolylines.Add(Pline2);
						Result.NegativePolylines.Add(Pline1);
						Result.ResultInfo = EBooleanResultInfo::Pline1InsidePline2;
					}
					else if (IsPline2InPline1())
					{
						Result.PositivePolylines.Add(Pline1);
						Result.NegativePolylines.Add(Pline2);
						Result.ResultInfo = EBooleanResultInfo::Pline2InsidePline1;
					}
					else
					{
						Result.PositivePolylines.Add(Pline1);
						Result.PositivePolylines.Add(Pline2);
						Result.ResultInfo = EBooleanResultInfo::Disjoint;
					}
				}
				else
				{
					// XOR = (Pline1 NOT Pline2) UNION (Pline2 NOT Pline1)
					Internal::FPrunedSlices Pruned1 = Internal::PruneSlices(Pline1, Pline2, BooleanInfo, EPCGExCCBooleanOp::Difference, PosEqualEps);
					TArray<FPolyline> Remaining1 = StitchSlicesIntoClosedPolylines(
						Pruned1, Pline1, Pline2, EPCGExCCBooleanOp::Difference, PosEqualEps, CollapsedAreaEps);

					// Swap and do second pass (Pline2 NOT Pline1)
					Internal::FProcessedBoolean BooleanInfo2 = Internal::ProcessForBoolean(Pline2, Pline1, PosEqualEps);
					Internal::FPrunedSlices Pruned2 = PruneSlices(Pline2, Pline1, BooleanInfo2, EPCGExCCBooleanOp::Difference, PosEqualEps);
					TArray<FPolyline> Remaining2 = StitchSlicesIntoClosedPolylines(
						Pruned2, Pline2, Pline1, EPCGExCCBooleanOp::Difference, PosEqualEps, CollapsedAreaEps);

					Result.PositivePolylines.Append(MoveTemp(Remaining1));
					Result.PositivePolylines.Append(MoveTemp(Remaining2));
					Result.ResultInfo = EBooleanResultInfo::Intersected;
				}
				break;
			}

			return Result;
		}
	}
}
