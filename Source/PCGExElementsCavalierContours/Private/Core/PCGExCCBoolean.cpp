// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours) - Boolean Operations

#include "Core/PCGExCCBoolean.h"

#include "Core/PCGExCCCommon.h"
#include "Details/PCGExCCDetails.h"
#include "Core/PCGExCCPolyline.h"
#include "Core/PCGExCCSegmentIntersect.h"

namespace PCGExCavalier::BooleanOps
{
	namespace Internal
	{
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
			int32 SourcePathId; // Path ID of the source polyline

			FVector2D GetStartPoint() const { return UpdatedStart.GetPosition(); }
		};

		/** Result of processing polylines for boolean operations */
		struct FProcessedBoolean
		{
			TArray<FBasicIntersect> Intersects;
			EPCGExCCOrientation Pline1Orientation;
			EPCGExCCOrientation Pline2Orientation;

			bool HasAnyIntersects() const
			{
				return Intersects.Num() > 0;
			}
		};

		/** Pruned slices ready for stitching */
		struct FPrunedSlices
		{
			TArray<FBooleanSlice> Slices;
			int32 StartOfPline2Slices = 0;
		};


		// Find all intersections between two polylines


		FIntersectsCollection FindAllIntersects(
			const FPolyline& Pline1,
			const FPolyline& Pline2,
			double PosEqualEps)
		{
			FIntersectsCollection Result;

			const int32 N1 = Pline1.SegmentCount();
			const int32 N2 = Pline2.SegmentCount();

			// Build spatial index for pline2
			FPolyline::FApproxAABBIndex Index2 = Pline2.CreateApproxAABBIndex();

			for (int32 i = 0; i < N1; ++i)
			{
				const FVertex& V1 = Pline1.GetVertex(i);
				const FVertex& V2 = Pline1.GetVertexWrapped(i + 1);

				// Compute AABB for segment
				double MinX = FMath::Min(V1.GetX(), V2.GetX());
				double MinY = FMath::Min(V1.GetY(), V2.GetY());
				double MaxX = FMath::Max(V1.GetX(), V2.GetX());
				double MaxY = FMath::Max(V1.GetY(), V2.GetY());

				// Expand for arc
				if (!V1.IsLine())
				{
					const double ChordLen = FVector2D::Distance(V1.GetPosition(), V2.GetPosition());
					const double Sagitta = FMath::Abs(V1.Bulge) * ChordLen * 0.5;
					MinX -= Sagitta;
					MinY -= Sagitta;
					MaxX += Sagitta;
					MaxY += Sagitta;
				}

				// Query potential intersecting segments
				Index2.Query(MinX, MinY, MaxX, MaxY, [&](int32 j)
				{
					const FVertex& U1 = Pline2.GetVertex(j);
					const FVertex& U2 = Pline2.GetVertexWrapped(j + 1);

					FPlineSegIntersect Intr = PlineSegmentIntersect(V1, V2, U1, U2, PosEqualEps);

					switch (Intr.Type)
					{
					case EPlineSegIntersectType::OneIntersect:
					case EPlineSegIntersectType::TangentIntersect:
						Result.BasicIntersects.Add(FBasicIntersect(i, j, Intr.Point1));
						break;

					case EPlineSegIntersectType::TwoIntersects:
						// Order by distance from V1
						{
							FVector2D P1 = Intr.Point1;
							FVector2D P2 = Intr.Point2;
							if (Math::DistanceSquared(V1.GetPosition(), P1) >
								Math::DistanceSquared(V1.GetPosition(), P2))
							{
								Swap(P1, P2);
							}
							Result.BasicIntersects.Add(FBasicIntersect(i, j, P1));
							Result.BasicIntersects.Add(FBasicIntersect(i, j, P2));
						}
						break;

					case EPlineSegIntersectType::OverlappingLines:
					case EPlineSegIntersectType::OverlappingArcs:
						Result.OverlappingIntersects.Add(
							FOverlappingIntersect(i, j, Intr.Point1, Intr.Point2));
						break;

					default:
						break;
					}
				});
			}

			return Result;
		}


		// Process polylines for boolean operation


		FProcessedBoolean ProcessForBoolean(
			const FPolyline& Pline1,
			const FPolyline& Pline2,
			double PosEqualEps)
		{
			FProcessedBoolean Result;

			Result.Pline1Orientation = Pline1.Orientation();
			Result.Pline2Orientation = Pline2.Orientation();

			FIntersectsCollection Intersects = FindAllIntersects(Pline1, Pline2, PosEqualEps);

			// Convert to basic intersects (simplified - full impl handles overlapping)
			Result.Intersects = MoveTemp(Intersects.BasicIntersects);

			return Result;
		}


		// Create slices from polyline between intersection points


		void CreateSlicesFromPline(
			const FPolyline& Pline,
			const TArray<FBasicIntersect>& Intersects,
			bool bIsPline1,
			int32 PathId,
			TArray<FBooleanSlice>& OutSlices,
			double PosEqualEps)
		{
			// Group intersects by segment index
			TMap<int32, TArray<const FBasicIntersect*>> IntersectsBySegment;

			for (const FBasicIntersect& Intr : Intersects)
			{
				const int32 SegIdx = bIsPline1 ? Intr.StartIndex1 : Intr.StartIndex2;
				IntersectsBySegment.FindOrAdd(SegIdx).Add(&Intr);
			}

			// Sort intersects on each segment by parameter
			for (auto& Pair : IntersectsBySegment)
			{
				const int32 SegIdx = Pair.Key;
				const FVector2D SegStart = Pline.GetVertex(SegIdx).GetPosition();

				Pair.Value.Sort([&SegStart](const FBasicIntersect& A, const FBasicIntersect& B)
				{
					return Math::DistanceSquared(SegStart, A.Point) < Math::DistanceSquared(SegStart, B.Point);
				});
			}

			// Flatten all intersection points in order around the polyline
			struct FIntrOnPline
			{
				int32 SegIndex;
				FVector2D Point;
				int32 OrigIntrIndex;
			};
			TArray<FIntrOnPline> OrderedIntrs;

			for (int32 i = 0; i < Pline.SegmentCount(); ++i)
			{
				TArray<const FBasicIntersect*>* SegIntrs = IntersectsBySegment.Find(i);
				if (SegIntrs)
				{
					for (const FBasicIntersect* Intr : *SegIntrs)
					{
						FIntrOnPline Entry;
						Entry.SegIndex = i;
						Entry.Point = Intr->Point;
						Entry.OrigIntrIndex = OrderedIntrs.Num();
						OrderedIntrs.Add(Entry);
					}
				}
			}

			if (OrderedIntrs.IsEmpty())
			{
				return;
			}

			// Create slices between consecutive intersection points
			for (int32 i = 0; i < OrderedIntrs.Num(); ++i)
			{
				const FIntrOnPline& Start = OrderedIntrs[i];
				const FIntrOnPline& End = OrderedIntrs[(i + 1) % OrderedIntrs.Num()];

				FBooleanSlice Slice;
				Slice.bSourceIsPline1 = bIsPline1;
				Slice.bInvertedDirection = false;
				Slice.bIsOverlapping = false;
				Slice.SourcePathId = PathId;

				// Update start vertex with intersection point
				const FVertex& OrigStartV = Pline.GetVertex(Start.SegIndex);
				Slice.StartIndex = Start.SegIndex;
				Slice.UpdatedStart = FVertex(Start.Point, OrigStartV.Bulge, OrigStartV.Source);

				// Compute end
				Slice.EndPoint = End.Point;

				// Compute end index offset (wrapping distance)
				if (Start.SegIndex == End.SegIndex)
				{
					// Same segment - check if we wrap around
					const double StartDist = Math::DistanceSquared(Pline.GetVertex(Start.SegIndex).GetPosition(), Start.Point);
					const double EndDist = Math::DistanceSquared(Pline.GetVertex(Start.SegIndex).GetPosition(), End.Point);

					if (EndDist > StartDist + PosEqualEps * PosEqualEps) { Slice.EndIndexOffset = 0; }
					else { Slice.EndIndexOffset = Pline.VertexCount(); }
				}
				else
				{
					int32 Offset = End.SegIndex - Start.SegIndex;
					if (Offset < 0)
					{
						Offset += Pline.VertexCount();
					}
					Slice.EndIndexOffset = Offset;
				}

				// Compute bulge for segment ending at intersection
				const int32 EndSegIdx = (Start.SegIndex + Slice.EndIndexOffset) % Pline.VertexCount();
				Slice.UpdatedEndBulge = Pline.GetVertex(EndSegIdx).Bulge;

				// If intersection is at segment end, may need to adjust bulge
				if (End.Point.Equals(Pline.GetVertexWrapped(EndSegIdx + 1).GetPosition(), PosEqualEps))
				{
					Slice.UpdatedEndBulge = Pline.GetVertex(EndSegIdx).Bulge;
				}

				OutSlices.Add(Slice);
			}
		}


		// Prune slices based on boolean operation type


		FPrunedSlices PruneSlices(
			const FPolyline& Pline1,
			const FPolyline& Pline2,
			int32 Pline1PathId,
			int32 Pline2PathId,
			const FProcessedBoolean& BoolInfo,
			EPCGExCCBooleanOp Operation,
			double PosEqualEps)
		{
			FPrunedSlices Result;

			// Create slices for both polylines
			TArray<FBooleanSlice> Pline1Slices;
			TArray<FBooleanSlice> Pline2Slices;

			CreateSlicesFromPline(Pline1, BoolInfo.Intersects, true, Pline1PathId, Pline1Slices, PosEqualEps);
			CreateSlicesFromPline(Pline2, BoolInfo.Intersects, false, Pline2PathId, Pline2Slices, PosEqualEps);

			// Determine which slices to keep based on operation
			auto KeepSlice = [&](const FBooleanSlice& Slice) -> bool
			{
				// Get slice midpoint
				const FPolyline& SourcePline = Slice.bSourceIsPline1 ? Pline1 : Pline2;
				const FPolyline& OtherPline = Slice.bSourceIsPline1 ? Pline2 : Pline1;

				// Compute midpoint of slice
				FVector2D MidPt = (Slice.GetStartPoint() + Slice.EndPoint) * 0.5;

				// If slice has vertices in between, use one of them
				if (Slice.EndIndexOffset > 1)
				{
					const int32 MidIdx = (Slice.StartIndex + Slice.EndIndexOffset / 2) % SourcePline.VertexCount();
					MidPt = SourcePline.GetVertex(MidIdx).GetPosition();
				}

				const bool bMidPtInOther = OtherPline.WindingNumber(MidPt) != 0;

				switch (Operation)
				{
				case EPCGExCCBooleanOp::Union:
					// Keep slices outside the other polyline
					return !bMidPtInOther;

				case EPCGExCCBooleanOp::Intersection:
					// Keep slices inside the other polyline
					return bMidPtInOther;

				case EPCGExCCBooleanOp::Difference:
					{
						if (Slice.bSourceIsPline1)
						{
							// Keep pline1 slices outside pline2
							return !bMidPtInOther;
						}
						// Keep pline2 slices inside pline1 (inverted)
						return bMidPtInOther;
					}

				case EPCGExCCBooleanOp::Xor:
					// Keep all slices (will be processed in two passes)
					return true;

				default:
					return true;
				}
			};

			// Filter slices
			for (const FBooleanSlice& Slice : Pline1Slices)
			{
				if (KeepSlice(Slice)) { Result.Slices.Add(Slice); }
			}

			Result.StartOfPline2Slices = Result.Slices.Num();

			for (FBooleanSlice Slice : Pline2Slices)
			{
				// For difference, invert pline2 slices
				if (Operation == EPCGExCCBooleanOp::Difference) { Slice.bInvertedDirection = true; }
				if (KeepSlice(Slice)) { Result.Slices.Add(Slice); }
			}

			return Result;
		}


		// Extend polyline from slice vertices


		void ExtendPolylineFromSlice(
			FPolyline& Pline,
			const FBooleanSlice& Slice,
			const FPolyline& Pline1,
			const FPolyline& Pline2,
			double PosEqualEps)
		{
			const FPolyline& SourcePline = Slice.bSourceIsPline1 ? Pline1 : Pline2;
			const int32 N = SourcePline.VertexCount();

			if (Slice.bInvertedDirection)
			{
				// Add vertices in reverse order
				for (int32 i = Slice.EndIndexOffset; i >= 0; --i)
				{
					const int32 Idx = (Slice.StartIndex + i) % N;
					const FVertex& V = SourcePline.GetVertex(Idx);

					// Adjust bulge for reversed direction
					FVertex AdjustedV = V.WithBulge(-V.Bulge);
					AdjustedV.Source = FVertexSource(Slice.SourcePathId, V.Source.PointIndex);

					Pline.AddOrReplaceVertex(AdjustedV, PosEqualEps);
				}
			}
			else
			{
				// Add start vertex with updated position
				FVertex StartV = Slice.UpdatedStart;
				StartV.Source = FVertexSource(Slice.SourcePathId, StartV.Source.PointIndex);
				Pline.AddOrReplaceVertex(StartV, PosEqualEps);

				// Add intermediate vertices
				for (int32 i = 1; i <= Slice.EndIndexOffset; ++i)
				{
					const int32 Idx = (Slice.StartIndex + i) % N;
					const FVertex& V = SourcePline.GetVertex(Idx);

					FVertex AdjustedV = V;
					AdjustedV.Source = FVertexSource(Slice.SourcePathId, V.Source.PointIndex);
					Pline.AddOrReplaceVertex(AdjustedV, PosEqualEps);
				}
			}

			// Add end point
			FVertex EndV(Slice.EndPoint, Slice.UpdatedEndBulge);
			EndV.Source = FVertexSource::FromPath(Slice.SourcePathId);
			Pline.AddOrReplaceVertex(EndV, PosEqualEps);
		}


		// Stitch slices into closed polylines


		TArray<FPolyline> StitchSlicesIntoClosedPolylines(
			const FPrunedSlices& PrunedSlices,
			const FPolyline& Pline1,
			const FPolyline& Pline2,
			int32 Pline1PathId,
			int32 Pline2PathId,
			EPCGExCCBooleanOp Operation,
			double PosEqualEps,
			double CollapsedAreaEps)
		{
			TArray<FPolyline> Results;

			if (PrunedSlices.Slices.IsEmpty()) { return Results; }

			TArray<bool> VisitedSliceIdx;
			VisitedSliceIdx.SetNumZeroed(PrunedSlices.Slices.Num());

			for (int32 BeginningSliceIdx = 0; BeginningSliceIdx < PrunedSlices.Slices.Num(); ++BeginningSliceIdx)
			{
				if (VisitedSliceIdx[BeginningSliceIdx]) { continue; }

				FPolyline CurrentPline(false);
				CurrentPline.AddContributingPath(Pline1PathId);
				CurrentPline.AddContributingPath(Pline2PathId);

				int32 CurrentSliceIdx = BeginningSliceIdx;
				VisitedSliceIdx[CurrentSliceIdx] = true;

				// Start with first slice
				ExtendPolylineFromSlice(CurrentPline, PrunedSlices.Slices[CurrentSliceIdx], Pline1, Pline2, PosEqualEps);

				// Continue stitching until we loop back
				while (true)
				{
					const FBooleanSlice& CurrentSlice = PrunedSlices.Slices[CurrentSliceIdx];
					const FVector2D SearchPoint = CurrentSlice.EndPoint;

					// Find connecting slice
					int32 ConnectedSliceIdx = INDEX_NONE;
					double MinDistSq = PosEqualEps * PosEqualEps * 4.0; // Allow some tolerance

					for (int32 j = 0; j < PrunedSlices.Slices.Num(); ++j)
					{
						if (j == CurrentSliceIdx){ continue; }

						const FBooleanSlice& CandidateSlice = PrunedSlices.Slices[j];
						const double DistSq = Math::DistanceSquared(SearchPoint, CandidateSlice.GetStartPoint());

						if (DistSq < MinDistSq)
						{
							MinDistSq = DistSq;
							ConnectedSliceIdx = j;
						}
					}

					if (ConnectedSliceIdx == INDEX_NONE) { break; }

					if (ConnectedSliceIdx == BeginningSliceIdx)
					{
						// Connected back to beginning - close and add
						if (CurrentPline.VertexCount() >= 3)
						{
							// Check if start connects to end
							if (CurrentPline.GetVertex(0).GetPosition().Equals(
								CurrentPline.LastVertex().GetPosition(), PosEqualEps))
							{
								CurrentPline.RemoveLastVertex();
							}

							// Close polyline
							FPolyline ClosedPline(true);
							ClosedPline.AddContributingPath(Pline1PathId);
							ClosedPline.AddContributingPath(Pline2PathId);

							for (int32 v = 0; v < CurrentPline.VertexCount(); ++v)
							{
								ClosedPline.AddVertex(CurrentPline.GetVertex(v));
							}

							// Check area threshold
							if (CollapsedAreaEps <= 0.0 || FMath::Abs(ClosedPline.Area()) >= CollapsedAreaEps)
							{
								ClosedPline.CollectPathIdsFromVertices();
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


	// Main Boolean Operation with path tracking


	FBooleanResult PerformBoolean(
		const FBooleanOperand& Operand1,
		const FBooleanOperand& Operand2,
		EPCGExCCBooleanOp Operation,
		const FPCGExContourBooleanOptions& Options)
	{
		FBooleanResult Result;

		// Validate input
		if (!Operand1.IsValid() || !Operand2.IsValid() ||
			!Operand1.Polyline->IsClosed() || !Operand2.Polyline->IsClosed())
		{
			Result.ResultInfo = EBooleanResultInfo::InvalidInput;
			return Result;
		}

		const FPolyline& Pline1 = *Operand1.Polyline;
		const FPolyline& Pline2 = *Operand2.Polyline;
		const int32 PathId1 = Operand1.PathId;
		const int32 PathId2 = Operand2.PathId;

		const double PosEqualEps = Options.PositionEqualEpsilon;
		const double CollapsedAreaEps = Options.CollapsedAreaEpsilon;

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

		// Create result polyline with source tracking
		auto CreateTrackedCopy = [](const FPolyline& Source, int32 PathId) -> FPolyline
		{
			FPolyline Copy(Source.IsClosed(), PathId);
			Copy.Reserve(Source.VertexCount());
			for (int32 i = 0; i < Source.VertexCount(); ++i)
			{
				FVertex V = Source.GetVertex(i);
				if (!V.HasValidPath())
				{
					V.Source = FVertexSource::FromPath(PathId);
				}
				Copy.AddVertex(V);
			}
			return Copy;
		};

		// Handle each operation
		switch (Operation)
		{
		case EPCGExCCBooleanOp::Union:
			if (!BooleanInfo.HasAnyIntersects())
			{
				if (IsPline1InPline2())
				{
					Result.PositivePolylines.Add(CreateTrackedCopy(Pline2, PathId2));
					Result.ResultInfo = EBooleanResultInfo::Pline1InsidePline2;
				}
				else if (IsPline2InPline1())
				{
					Result.PositivePolylines.Add(CreateTrackedCopy(Pline1, PathId1));
					Result.ResultInfo = EBooleanResultInfo::Pline2InsidePline1;
				}
				else
				{
					Result.PositivePolylines.Add(CreateTrackedCopy(Pline1, PathId1));
					Result.PositivePolylines.Add(CreateTrackedCopy(Pline2, PathId2));
					Result.ResultInfo = EBooleanResultInfo::Disjoint;
				}
			}
			else
			{
				Internal::FPrunedSlices Pruned = Internal::PruneSlices(
					Pline1, Pline2, PathId1, PathId2, BooleanInfo, Operation, PosEqualEps);

				TArray<FPolyline> Remaining = Internal::StitchSlicesIntoClosedPolylines(
					Pruned, Pline1, Pline2, PathId1, PathId2, Operation, PosEqualEps, CollapsedAreaEps);

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
			if (!BooleanInfo.HasAnyIntersects())
			{
				if (IsPline1InPline2())
				{
					Result.PositivePolylines.Add(CreateTrackedCopy(Pline1, PathId1));
					Result.ResultInfo = EBooleanResultInfo::Pline1InsidePline2;
				}
				else if (IsPline2InPline1())
				{
					Result.PositivePolylines.Add(CreateTrackedCopy(Pline2, PathId2));
					Result.ResultInfo = EBooleanResultInfo::Pline2InsidePline1;
				}
				else
				{
					Result.ResultInfo = EBooleanResultInfo::Disjoint;
				}
			}
			else
			{
				Internal::FPrunedSlices Pruned = Internal::PruneSlices(
					Pline1, Pline2, PathId1, PathId2, BooleanInfo, Operation, PosEqualEps);

				Result.PositivePolylines = Internal::StitchSlicesIntoClosedPolylines(
					Pruned, Pline1, Pline2, PathId1, PathId2, Operation, PosEqualEps, CollapsedAreaEps);

				Result.ResultInfo = EBooleanResultInfo::Intersected;
			}
			break;

		case EPCGExCCBooleanOp::Difference:
			if (!BooleanInfo.HasAnyIntersects())
			{
				if (IsPline1InPline2())
				{
					Result.ResultInfo = EBooleanResultInfo::Pline1InsidePline2;
				}
				else if (IsPline2InPline1())
				{
					Result.PositivePolylines.Add(CreateTrackedCopy(Pline1, PathId1));
					Result.NegativePolylines.Add(CreateTrackedCopy(Pline2, PathId2));
					Result.ResultInfo = EBooleanResultInfo::Pline2InsidePline1;
				}
				else
				{
					Result.PositivePolylines.Add(CreateTrackedCopy(Pline1, PathId1));
					Result.ResultInfo = EBooleanResultInfo::Disjoint;
				}
			}
			else
			{
				Internal::FPrunedSlices Pruned = Internal::PruneSlices(
					Pline1, Pline2, PathId1, PathId2, BooleanInfo, Operation, PosEqualEps);

				Result.PositivePolylines = Internal::StitchSlicesIntoClosedPolylines(
					Pruned, Pline1, Pline2, PathId1, PathId2, Operation, PosEqualEps, CollapsedAreaEps);

				Result.ResultInfo = EBooleanResultInfo::Intersected;
			}
			break;

		case EPCGExCCBooleanOp::Xor:
			if (!BooleanInfo.HasAnyIntersects())
			{
				if (IsPline1InPline2())
				{
					Result.PositivePolylines.Add(CreateTrackedCopy(Pline2, PathId2));
					Result.NegativePolylines.Add(CreateTrackedCopy(Pline1, PathId1));
					Result.ResultInfo = EBooleanResultInfo::Pline1InsidePline2;
				}
				else if (IsPline2InPline1())
				{
					Result.PositivePolylines.Add(CreateTrackedCopy(Pline1, PathId1));
					Result.NegativePolylines.Add(CreateTrackedCopy(Pline2, PathId2));
					Result.ResultInfo = EBooleanResultInfo::Pline2InsidePline1;
				}
				else
				{
					Result.PositivePolylines.Add(CreateTrackedCopy(Pline1, PathId1));
					Result.PositivePolylines.Add(CreateTrackedCopy(Pline2, PathId2));
					Result.ResultInfo = EBooleanResultInfo::Disjoint;
				}
			}
			else
			{
				// XOR = (Pline1 - Pline2) UNION (Pline2 - Pline1)
				Internal::FPrunedSlices Pruned1 = Internal::PruneSlices(
					Pline1, Pline2, PathId1, PathId2, BooleanInfo, EPCGExCCBooleanOp::Difference, PosEqualEps);

				TArray<FPolyline> Remaining1 = Internal::StitchSlicesIntoClosedPolylines(
					Pruned1, Pline1, Pline2, PathId1, PathId2, EPCGExCCBooleanOp::Difference, PosEqualEps, CollapsedAreaEps);

				// Swap and do second pass
				Internal::FProcessedBoolean BooleanInfo2 = Internal::ProcessForBoolean(Pline2, Pline1, PosEqualEps);
				Internal::FPrunedSlices Pruned2 = Internal::PruneSlices(
					Pline2, Pline1, PathId2, PathId1, BooleanInfo2, EPCGExCCBooleanOp::Difference, PosEqualEps);

				TArray<FPolyline> Remaining2 = Internal::StitchSlicesIntoClosedPolylines(
					Pruned2, Pline2, Pline1, PathId2, PathId1, EPCGExCCBooleanOp::Difference, PosEqualEps, CollapsedAreaEps);

				Result.PositivePolylines.Append(MoveTemp(Remaining1));
				Result.PositivePolylines.Append(MoveTemp(Remaining2));
				Result.ResultInfo = EBooleanResultInfo::Intersected;
			}
			break;
		}

		// Collect all contributing path IDs
		Result.AllContributingPathIds.Add(PathId1);
		Result.AllContributingPathIds.Add(PathId2);
		Result.CollectContributingPathIds();

		return Result;
	}

	FBooleanResult PerformBoolean(
		const FPolyline& Pline1,
		const FPolyline& Pline2,
		EPCGExCCBooleanOp Operation,
		const FPCGExContourBooleanOptions& Options)
	{
		// Use polylines' primary path IDs, defaulting to 0 and 1
		const int32 PathId1 = Pline1.GetPrimaryPathId() != INDEX_NONE ? Pline1.GetPrimaryPathId() : 0;
		const int32 PathId2 = Pline2.GetPrimaryPathId() != INDEX_NONE ? Pline2.GetPrimaryPathId() : 1;

		return PerformBoolean(
			FBooleanOperand(Pline1, PathId1),
			FBooleanOperand(Pline2, PathId2),
			Operation,
			Options);
	}

	FBooleanResult UnionAll(
		const TArray<FBooleanOperand>& Operands,
		const FPCGExContourBooleanOptions& Options)
	{
		FBooleanResult Result;

		if (Operands.IsEmpty())
		{
			Result.ResultInfo = EBooleanResultInfo::InvalidInput;
			return Result;
		}

		if (Operands.Num() == 1)
		{
			if (Operands[0].IsValid())
			{
				FPolyline Copy = *Operands[0].Polyline;
				Copy.SetPrimaryPathId(Operands[0].PathId);
				Result.PositivePolylines.Add(MoveTemp(Copy));
				Result.AllContributingPathIds.Add(Operands[0].PathId);
				Result.ResultInfo = EBooleanResultInfo::Disjoint;
			}
			else
			{
				Result.ResultInfo = EBooleanResultInfo::InvalidInput;
			}
			return Result;
		}

		// Start with first operand
		FBooleanResult Current;
		FPolyline FirstCopy = *Operands[0].Polyline;
		FirstCopy.SetPrimaryPathId(Operands[0].PathId);
		Current.PositivePolylines.Add(MoveTemp(FirstCopy));
		Current.AllContributingPathIds.Add(Operands[0].PathId);

		// Union with each subsequent operand
		for (int32 i = 1; i < Operands.Num(); ++i)
		{
			if (!Operands[i].IsValid())
			{
				continue;
			}

			TArray<FPolyline> NextPositive;
			TArray<FPolyline> NextNegative;

			for (const FPolyline& Positive : Current.PositivePolylines)
			{
				FBooleanResult PartialResult = PerformBoolean(
					FBooleanOperand(Positive, Positive.GetPrimaryPathId()),
					Operands[i],
					EPCGExCCBooleanOp::Union,
					Options);

				NextPositive.Append(MoveTemp(PartialResult.PositivePolylines));
				NextNegative.Append(MoveTemp(PartialResult.NegativePolylines));
			}

			Current.PositivePolylines = MoveTemp(NextPositive);
			Current.NegativePolylines.Append(MoveTemp(NextNegative));
			Current.AllContributingPathIds.Add(Operands[i].PathId);
		}

		Result = MoveTemp(Current);
		Result.ResultInfo = Result.HasResult() ? EBooleanResultInfo::Intersected : EBooleanResultInfo::Disjoint;
		Result.CollectContributingPathIds();

		return Result;
	}

	FBooleanResult IntersectAll(
		const TArray<FBooleanOperand>& Operands,
		const FPCGExContourBooleanOptions& Options)
	{
		FBooleanResult Result;

		if (Operands.IsEmpty())
		{
			Result.ResultInfo = EBooleanResultInfo::InvalidInput;
			return Result;
		}

		if (Operands.Num() == 1)
		{
			if (Operands[0].IsValid())
			{
				FPolyline Copy = *Operands[0].Polyline;
				Copy.SetPrimaryPathId(Operands[0].PathId);
				Result.PositivePolylines.Add(MoveTemp(Copy));
				Result.AllContributingPathIds.Add(Operands[0].PathId);
				Result.ResultInfo = EBooleanResultInfo::Disjoint;
			}
			else
			{
				Result.ResultInfo = EBooleanResultInfo::InvalidInput;
			}
			return Result;
		}

		// Start with first operand
		FBooleanResult Current;
		FPolyline FirstCopy = *Operands[0].Polyline;
		FirstCopy.SetPrimaryPathId(Operands[0].PathId);
		Current.PositivePolylines.Add(MoveTemp(FirstCopy));
		Current.AllContributingPathIds.Add(Operands[0].PathId);

		// Intersect with each subsequent operand
		for (int32 i = 1; i < Operands.Num(); ++i)
		{
			if (!Operands[i].IsValid())
			{
				continue;
			}

			TArray<FPolyline> NextPositive;

			for (const FPolyline& Positive : Current.PositivePolylines)
			{
				FBooleanResult PartialResult = PerformBoolean(
					FBooleanOperand(Positive, Positive.GetPrimaryPathId()),
					Operands[i],
					EPCGExCCBooleanOp::Intersection,
					Options);

				NextPositive.Append(MoveTemp(PartialResult.PositivePolylines));
			}

			Current.PositivePolylines = MoveTemp(NextPositive);
			Current.AllContributingPathIds.Add(Operands[i].PathId);

			// Early exit if no intersection
			if (Current.PositivePolylines.IsEmpty())
			{
				Result.ResultInfo = EBooleanResultInfo::Disjoint;
				return Result;
			}
		}

		Result = MoveTemp(Current);
		Result.ResultInfo = Result.HasResult() ? EBooleanResultInfo::Intersected : EBooleanResultInfo::Disjoint;
		Result.CollectContributingPathIds();

		return Result;
	}
}
