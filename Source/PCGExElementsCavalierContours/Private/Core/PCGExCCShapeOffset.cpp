// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours) - Shape Offset

#include "Core/PCGExCCShapeOffset.h"
#include "Core/PCGExCCSegmentIntersect.h"

namespace PCGExCavalier::ShapeOffset
{
	FShape FShape::FromPolylines(const TArray<FPolyline>& Polylines)
	{
		FShape Result;

		for (int32 i = 0; i < Polylines.Num(); ++i)
		{
			const FPolyline& Pline = Polylines[i];

			// Skip empty or invalid polylines
			if (Pline.VertexCount() < 3) continue;

			const double Area = Pline.Area();
			const int32 PrimaryPathId = Pline.GetPrimaryPathId();
			const int32 PathId = PrimaryPathId != INDEX_NONE ? PrimaryPathId : i;

			if (Area >= 0.0)
			{
				// Counter-clockwise (positive area)
				Result.CCWPolylines.Add(FIndexedPolyline(Pline));
				Result.CCWPathIds.Add(PathId);
			}
			else
			{
				// Clockwise (negative area / hole)
				Result.CWPolylines.Add(FIndexedPolyline(Pline));
				Result.CWPathIds.Add(PathId);
			}
		}

		return Result;
	}

	TArray<FPolyline> FShape::GetAllPolylines() const
	{
		TArray<FPolyline> Result;
		Result.Reserve(CCWPolylines.Num() + CWPolylines.Num());

		for (const FIndexedPolyline& IP : CCWPolylines)
		{
			Result.Add(IP.Polyline);
		}
		for (const FIndexedPolyline& IP : CWPolylines)
		{
			Result.Add(IP.Polyline);
		}

		return Result;
	}

	FShape FShape::ParallelOffset(double Offset, const FShapeOffsetOptions& Options) const
	{
		// Step 1: Create offset loops
		TArray<FOffsetLoop> CCWOffsetLoops;
		TArray<FOffsetLoop> CWOffsetLoops;
		CreateOffsetLoopsWithIndex(Offset, Options, CCWOffsetLoops, CWOffsetLoops);

		if (CCWOffsetLoops.IsEmpty() && CWOffsetLoops.IsEmpty()) { return FShape::Empty(); }

		// Step 2: Find intersections between offset loops
		TArray<FSlicePointSet> SlicePointSets;
		FindIntersectsBetweenOffsetLoops(CCWOffsetLoops, CWOffsetLoops, Options.PosEqualEps, SlicePointSets);

		// Step 3: Create valid slices from intersection points
		TArray<FDissectedSlice> SlicesData;
		CreateValidSlicesFromIntersects(CCWOffsetLoops, CWOffsetLoops, SlicePointSets, Offset, Options, SlicesData);

		// Step 4: Stitch slices together
		return StitchSlicesTogether(SlicesData, CCWOffsetLoops, CWOffsetLoops, Options.PosEqualEps, Options.SliceJoinEps);
	}

	void FShape::CreateOffsetLoopsWithIndex(
		double Offset,
		const FShapeOffsetOptions& Options,
		TArray<FOffsetLoop>& OutCCWOffsetLoops,
		TArray<FOffsetLoop>& OutCWOffsetLoops) const
	{
		FPCGExCCOffsetOptions OffsetOpts;
		OffsetOpts.PositionEqualEpsilon = Options.PosEqualEps;
		OffsetOpts.OffsetDistanceEpsilon = Options.OffsetDistEps;
		OffsetOpts.SliceJoinEpsilon = Options.SliceJoinEps;

		// FIX: Set this to TRUE. Individual loops must be self-resolved 
		// before inter-loop intersections are calculated.
		OffsetOpts.bHandleSelfIntersects = true;

		auto ProcessPolylineList = [&](const TArray<FIndexedPolyline>& SourceList, const TArray<int32>& IdList, int32& StartParentIdx)
		{
			for (int32 i = 0; i < SourceList.Num(); ++i)
			{
				const int32 PathId = i < IdList.Num() ? IdList[i] : StartParentIdx;

				// Generate offset for single polyline
				TArray<FPolyline> OffsetResults = Offset::ParallelOffset(SourceList[i].Polyline, Offset, OffsetOpts);

				for (FPolyline& OffsetPline : OffsetResults)
				{
					const double Area = OffsetPline.Area();
					// Skip collapsed loops where orientation flipped incorrectly
					if ((Offset > 0.0 && Area < 0.0 && SourceList[i].Polyline.Area() > 0.0) ||
						(Offset < 0.0 && Area > 0.0 && SourceList[i].Polyline.Area() < 0.0))
					{
						continue;
					}

					OffsetPline.SetPrimaryPathId(PathId);
					if (Area < 0.0)
					{
						OutCWOffsetLoops.Add(FOffsetLoop(StartParentIdx, PathId, MoveTemp(OffsetPline)));
					}
					else
					{
						OutCCWOffsetLoops.Add(FOffsetLoop(StartParentIdx, PathId, MoveTemp(OffsetPline)));
					}
				}
				StartParentIdx++;
			}
		};

		int32 GlobalParentIdx = 0;
		ProcessPolylineList(CCWPolylines, CCWPathIds, GlobalParentIdx);
		ProcessPolylineList(CWPolylines, CWPathIds, GlobalParentIdx);
	}

	void FShape::FindIntersectsBetweenOffsetLoops(
		const TArray<FOffsetLoop>& CCWOffsetLoops,
		const TArray<FOffsetLoop>& CWOffsetLoops,
		double PosEqualEps,
		TArray<FSlicePointSet>& OutSlicePointSets) const
	{
		const int32 TotalLoops = CCWOffsetLoops.Num() + CWOffsetLoops.Num();
		if (TotalLoops < 2) return;

		// Track visited pairs to avoid duplicates
		TSet<uint64> VisitedPairs;

		auto MakePairKey = [](int32 I, int32 J) -> uint64
		{
			const int32 Low = FMath::Min(I, J);
			const int32 High = FMath::Max(I, J);
			return (static_cast<uint64>(Low) << 32) | static_cast<uint64>(High);
		};

		// Buffer for intersection results
		Offset::FIntersectionBuffer IntersectionBuffer;

		for (int32 i = 0; i < TotalLoops; ++i)
		{
			const FOffsetLoop& Loop1 = GetLoop(i, CCWOffsetLoops, CWOffsetLoops);
			const FBox2D Bounds1 = Loop1.GetBounds();

			// Start from i+1 to avoid self-comparisons
			for (int32 j = i + 1; j < TotalLoops; ++j)
			{
				bool bAlreadyInSet = false;
				VisitedPairs.Add(MakePairKey(i, j), &bAlreadyInSet);
				if (bAlreadyInSet) continue;

				const FOffsetLoop& Loop2 = GetLoop(j, CCWOffsetLoops, CWOffsetLoops);
				const FBox2D Bounds2 = Loop2.GetBounds();

				// Quick AABB rejection
				if (!Bounds1.Intersect(Bounds2)) continue;

				// Find intersections between the two loops
				IntersectionBuffer.Reset();

				Offset::Internal::FindIntersectsBetween(
					Loop1.Polyline, Loop2.Polyline,
					Loop1.SpatialIndex, PosEqualEps,
					IntersectionBuffer);

				if (IntersectionBuffer.Num() == 0) continue;

				// Add a safety check to prevent malformed intersection data
				if (IntersectionBuffer.Num() > 0)
				{
					// Store intersection data - ensure we're not losing any intersections
					FSlicePointSet& Set = OutSlicePointSets.AddDefaulted_GetRef();
					Set.LoopIdx1 = i;
					Set.LoopIdx2 = j;
					Set.SlicePoints.Reserve(IntersectionBuffer.Num());

					for (int32 k = 0; k < IntersectionBuffer.Num(); ++k)
					{
						// Make sure we're copying the intersection properly
						// Add validation to prevent invalid intersections
						const FBasicIntersect& Intr = IntersectionBuffer[k];
						if (Intr.Point.X != 0 || Intr.Point.Y != 0) // Skip zero points if they exist
						{
							Set.SlicePoints.Add(Intr);
						}
					}
				}
			}
		}
	}

	void FShape::CreateValidSlicesFromIntersects(
		const TArray<FOffsetLoop>& CCWOffsetLoops,
		const TArray<FOffsetLoop>& CWOffsetLoops,
		const TArray<FSlicePointSet>& SlicePointSets,
		double Offset,
		const FShapeOffsetOptions& Options,
		TArray<FDissectedSlice>& OutSlicesData) const
	{
		const int32 TotalLoops = CCWOffsetLoops.Num() + CWOffsetLoops.Num();

		// Dissection point structure
		struct FDissectionPoint
		{
			int32 SegIdx;
			FVector2D Pos;
		};

		// 1. Build a map of all intersection points for each loop
		TMap<int32, TArray<FDissectionPoint>> LoopPointsMap;

		// Properly handle all slice point sets with better validation
		for (const FSlicePointSet& Set : SlicePointSets)
		{
			// Only process valid indices with bounds checking
			if (Set.LoopIdx1 < TotalLoops && Set.LoopIdx2 < TotalLoops)
			{
				for (const FBasicIntersect& Intr : Set.SlicePoints)
				{
					// Validate intersection point coordinates
					if (FMath::IsFinite(Intr.Point.X) && FMath::IsFinite(Intr.Point.Y))
					{
						// Add to Loop1 - ensure valid index
						if (Set.LoopIdx1 < TotalLoops)
						{
							FDissectionPoint& DP1 = LoopPointsMap.FindOrAdd(Set.LoopIdx1).AddDefaulted_GetRef();
							DP1.SegIdx = Intr.StartIndex1;
							DP1.Pos = Intr.Point;
						}

						// Add to Loop2 - ensure valid index  
						if (Set.LoopIdx2 < TotalLoops)
						{
							FDissectionPoint& DP2 = LoopPointsMap.FindOrAdd(Set.LoopIdx2).AddDefaulted_GetRef();
							DP2.SegIdx = Intr.StartIndex2;
							DP2.Pos = Intr.Point;
						}
					}
				}
			}
		}

		for (int32 LoopIdx = 0; LoopIdx < TotalLoops; ++LoopIdx)
		{
			const FOffsetLoop& CurrLoop = GetLoop(LoopIdx, CCWOffsetLoops, CWOffsetLoops);
			const FPolyline& LoopPline = CurrLoop.Polyline;
			const int32 VertCount = LoopPline.VertexCount();

			TArray<FDissectionPoint>* Points = LoopPointsMap.Find(LoopIdx);

			if (!Points || Points->Num() == 0)
			{
				// NO INTERSECTIONS: Test the whole loop as a single slice
				FDissectedSlice Slice(LoopIdx, 0, VertCount, LoopPline.GetVertex(0).GetPosition());
				Slice.EndPoint = LoopPline.GetVertex(0).GetPosition();

				// Ensure we test the validity of the full loop properly
				if (IsSliceValid(Slice, CurrLoop, Offset, Options))
				{
					OutSlicesData.Add(MoveTemp(Slice));
				}
				continue;
			}

			// 2. SORT and DEDUPLICATE points along the loop direction
			Points->Sort([&LoopPline](const FDissectionPoint& A, const FDissectionPoint& B)
			{
				if (A.SegIdx != B.SegIdx) return A.SegIdx < B.SegIdx;
				return FVector2D::DistSquared(LoopPline.GetVertex(A.SegIdx).GetPosition(), A.Pos) <
					FVector2D::DistSquared(LoopPline.GetVertex(B.SegIdx).GetPosition(), B.Pos);
			});

			// Remove duplicates properly - make sure we don't remove legitimate points
			for (int32 i = Points->Num() - 1; i > 0; --i)
			{
				if (FVector2D::DistSquared((*Points)[i].Pos, (*Points)[i - 1].Pos) < FMath::Square(Options.PosEqualEps))
					Points->RemoveAt(i);
			}

			// Handle special case for single point - but be careful
			if (Points->Num() == 1)
			{
				// For a single intersection point on a loop, treat as whole loop if valid
				FDissectedSlice Slice(LoopIdx, 0, VertCount, LoopPline.GetVertex(0).GetPosition());
				Slice.EndPoint = LoopPline.GetVertex(0).GetPosition();
				if (IsSliceValid(Slice, CurrLoop, Offset, Options))
				{
					OutSlicesData.Add(MoveTemp(Slice));
				}
				continue;
			}

			// 3. Create slices between consecutive intersection points
			auto CreateSlice = [&](const FDissectionPoint& Start, const FDissectionPoint& End)
			{
				// Validate that we have a reasonable slice
				if (Start.Pos.Equals(End.Pos, Options.PosEqualEps))
					return; // Skip zero-length slices

				FDissectedSlice Slice;
				Slice.SourceIdx = LoopIdx;
				Slice.StartIndex = Start.SegIdx;
				Slice.UpdatedStart = FVertex(Start.Pos, LoopPline.GetVertex(Start.SegIdx).Bulge);
				Slice.EndPoint = End.Pos;

				int32 Span = End.SegIdx - Start.SegIdx;
				if (Span < 0) Span += VertCount;
				Slice.EndIndexOffset = Span;

				// Additional safety check for slice length
				if (Slice.EndIndexOffset >= 0 && IsSliceValid(Slice, CurrLoop, Offset, Options))
				{
					OutSlicesData.Add(MoveTemp(Slice));
				}
			};

			// Internal slices - proper handling of consecutive points
			for (int32 i = 0; i < Points->Num() - 1; ++i)
			{
				CreateSlice((*Points)[i], (*Points)[i + 1]);
			}

			// Handle wrap-around connection properly for closed loops only
			if (Points->Num() > 1 && LoopPline.IsClosed())
			{
				CreateSlice(Points->Last(), (*Points)[0]);
			}
		}
	}

	bool FShape::IsSliceValid(
		const FDissectedSlice& Slice,
		const FOffsetLoop& OffsetLoop,
		double Offset,
		const FShapeOffsetOptions& Options) const
	{
		const FPolyline& LoopPline = OffsetLoop.Polyline;
		const int32 VertCount = LoopPline.VertexCount();

		// Helper to retrieve vertices as if the slice were its own polyline
		auto GetViewVertex = [&](int32 ViewIdx) -> FVertex
		{
			if (ViewIdx == 0) return Slice.UpdatedStart;

			const int32 RealIdx = (Slice.StartIndex + ViewIdx) % VertCount;
			return LoopPline.GetVertex(RealIdx);
		};

		FVector2D Midpoint1;
		TOptional<FVector2D> Midpoint2;

		// Replicate Rust logic: Avoid intersection points at start/end if possible
		const int32 NumPoints = Slice.EndIndexOffset + 1;

		if (NumPoints > 3)
		{
			// Use segment from index 1 to 2
			const FVertex V1 = GetViewVertex(1);
			const FVertex V2 = GetViewVertex(2);
			Midpoint1 = Math::ArcMidpoint(V1.GetPosition(), V2.GetPosition(), V1.Bulge);
		}
		else if (NumPoints == 3)
		{
			// Exactly 2 segments: test both midpoints
			const FVertex V0 = GetViewVertex(0);
			const FVertex V1 = GetViewVertex(1);
			const FVertex V2 = GetViewVertex(2);
			Midpoint1 = Math::ArcMidpoint(V0.GetPosition(), V1.GetPosition(), V0.Bulge);
			Midpoint2 = Math::ArcMidpoint(V1.GetPosition(), V2.GetPosition(), V1.Bulge);
		}
		else if (NumPoints == 2)
		{
			// Only 1 segment: use its midpoint
			const FVertex V0 = GetViewVertex(0);
			const FVertex V1 = GetViewVertex(1);
			Midpoint1 = Math::ArcMidpoint(V0.GetPosition(), V1.GetPosition(), V0.Bulge);
		}
		else
		{
			// Single point - this shouldn't happen but handle gracefully
			Midpoint1 = Slice.UpdatedStart.GetPosition();
		}

		const int32 TotalInputLoops = CCWPolylines.Num() + CWPolylines.Num();
		const int32 ParentIdx = OffsetLoop.ParentLoopIdx;
		const double AbsOffset = FMath::Abs(Offset);

		// Filter out parent_idx as per Rust logic
		for (int32 i = 0; i < TotalInputLoops; ++i)
		{
			if (i == ParentIdx) continue;

			const FIndexedPolyline& Target = (i < CCWPolylines.Num())
				                                 ? CCWPolylines[i]
				                                 : CWPolylines[i - CCWPolylines.Num()];

			// Check first midpoint
			if (!Offset::Internal::PointValidForOffset(
				Target.Polyline, Target.SpatialIndex, AbsOffset, Midpoint1,
				Options.PosEqualEps, Options.OffsetDistEps))
			{
				return false;
			}

			// Check second midpoint if it exists
			if (Midpoint2.IsSet() && !Offset::Internal::PointValidForOffset(
				Target.Polyline, Target.SpatialIndex, AbsOffset, Midpoint2.GetValue(),
				Options.PosEqualEps, Options.OffsetDistEps))
			{
				return false;
			}
		}

		return true;
	}

	FShape FShape::StitchSlicesTogether(
		TArray<FDissectedSlice>& SlicesData,
		const TArray<FOffsetLoop>& CCWOffsetLoops,
		const TArray<FOffsetLoop>& CWOffsetLoops,
		double PosEqualEps,
		double SliceJoinEps) const
	{
		if (SlicesData.IsEmpty())
		{
			return FShape::Empty();
		}

		FShape Result;
		TArray<bool> VisitedSlices;
		VisitedSlices.SetNumZeroed(SlicesData.Num());

		const double JoinEpsSq = SliceJoinEps * SliceJoinEps;
		const int32 MaxLoopCount = SlicesData.Num() * 10; // Much higher limit to prevent premature termination

		for (int32 SliceIdx = 0; SliceIdx < SlicesData.Num(); ++SliceIdx)
		{
			if (VisitedSlices[SliceIdx]) continue;
			VisitedSlices[SliceIdx] = true;

			int32 CurrentIndex = SliceIdx;
			int32 LoopCount = 0;
			FPolyline CurrentPline(false);

			// Track if we're building a closed loop or open
			bool bIsClosedLoop = false;

			while (LoopCount < MaxLoopCount)
			{
				++LoopCount;

				const FDissectedSlice& CurrSlice = SlicesData[CurrentIndex];
				const FOffsetLoop& SourceLoop = GetLoop(CurrSlice.SourceIdx, CCWOffsetLoops, CWOffsetLoops);
				const FPolyline& SourcePline = SourceLoop.Polyline;
				const int32 N = SourcePline.VertexCount();

				// Add vertices from this slice properly - don't add duplicates at the start
				if (CurrentPline.VertexCount() == 0)
				{
					CurrentPline.AddOrReplaceVertex(CurrSlice.UpdatedStart, PosEqualEps);
					CurrentPline.AddContributingPath(SourceLoop.ParentPathId);
				}
				else
				{
					// Only add if it's not already the last vertex
					const FVector2D LastPos = CurrentPline.LastVertex().GetPosition();
					if (!LastPos.Equals(CurrSlice.UpdatedStart.GetPosition(), PosEqualEps))
					{
						CurrentPline.AddOrReplaceVertex(CurrSlice.UpdatedStart, PosEqualEps);
					}
				}

				// Add intermediate vertices
				for (int32 j = 1; j <= CurrSlice.EndIndexOffset; ++j)
				{
					const int32 Idx = (CurrSlice.StartIndex + j) % N;
					CurrentPline.AddOrReplaceVertex(SourcePline.GetVertex(Idx), PosEqualEps);
				}

				// Add the endpoint as a final vertex - don't add if it's already there
				if (CurrentPline.VertexCount() > 0)
				{
					const FVector2D LastPos = CurrentPline.LastVertex().GetPosition();
					if (!LastPos.Equals(CurrSlice.EndPoint, PosEqualEps))
					{
						CurrentPline.AddOrReplaceVertex(FVertex(CurrSlice.EndPoint, 0.0), PosEqualEps);
					}
				}

				// Find connecting slice - more robust connection logic
				const FVector2D EndPoint = CurrSlice.EndPoint;
				int32 ConnectedSliceIdx = INDEX_NONE;
				double MinDistSq = JoinEpsSq * 4.0;

				for (int32 j = 0; j < SlicesData.Num(); ++j)
				{
					if (j == CurrentIndex || VisitedSlices[j]) continue;

					const double DistSq = Math::DistanceSquared(EndPoint, SlicesData[j].GetStartPoint());
					if (DistSq < MinDistSq)
					{
						// Prefer slices from the same source for better stitching
						if (SlicesData[j].SourceIdx == CurrSlice.SourceIdx || DistSq < JoinEpsSq * 2.0)
						{
							MinDistSq = DistSq;
							ConnectedSliceIdx = j;
						}
					}
				}

				if (ConnectedSliceIdx == INDEX_NONE)
				{
					// No more connections - close and finish this polyline
					if (CurrentPline.VertexCount() > 2)
					{
						// Only set closed if we have a valid closed loop
						const FVector2D FirstPos = CurrentPline.GetVertex(0).GetPosition();
						const FVector2D LastPos = CurrentPline.LastVertex().GetPosition();

						// Proper closure logic - only close when first and last match exactly
						bool bShouldClose = false;
						if (FirstPos.Equals(LastPos, PosEqualEps))
						{
							bShouldClose = true;
						}

						if (bShouldClose)
						{
							CurrentPline.RemoveLastVertex();
							CurrentPline.SetClosed(true);
							bIsClosedLoop = true;
						}

						// Classify by orientation
						const double Area = CurrentPline.Area();
						if (Area >= 0.0)
						{
							Result.CCWPolylines.Add(FIndexedPolyline(MoveTemp(CurrentPline)));
							Result.CCWPathIds.Add(SourceLoop.ParentPathId);
						}
						else
						{
							Result.CWPolylines.Add(FIndexedPolyline(MoveTemp(CurrentPline)));
							Result.CWPathIds.Add(SourceLoop.ParentPathId);
						}
					}
					break;
				}

				// Continue to next slice
				CurrentIndex = ConnectedSliceIdx;
				VisitedSlices[CurrentIndex] = true;
			}
		}

		return Result;
	}


	const FOffsetLoop& FShape::GetLoop(
		int32 Index,
		const TArray<FOffsetLoop>& CCWLoops,
		const TArray<FOffsetLoop>& CWLoops)
	{
		if (Index < CCWLoops.Num())
		{
			return CCWLoops[Index];
		}
		return CWLoops[Index - CCWLoops.Num()];
	}

	const FIndexedPolyline& FShape::GetIndexedPolyline(int32 Index) const
	{
		if (Index < CCWPolylines.Num())
		{
			return CCWPolylines[Index];
		}
		return CWPolylines[Index - CCWPolylines.Num()];
	}

	int32 FShape::GetPathId(int32 Index) const
	{
		if (Index < CCWPathIds.Num())
		{
			return CCWPathIds[Index];
		}
		const int32 CWIndex = Index - CCWPolylines.Num();
		return CWIndex < CWPathIds.Num() ? CWPathIds[CWIndex] : INDEX_NONE;
	}

	TArray<FPolyline> ParallelOffsetShape(const TArray<FPolyline>& Polylines, double Offset, const FShapeOffsetOptions& Options)
	{
		if (Polylines.IsEmpty()) return TArray<FPolyline>();

		FShape Shape = FShape::FromPolylines(Polylines);
		FShape Result = Shape.ParallelOffset(Offset, Options);
		return Result.GetAllPolylines();
	}

	TArray<FPolyline> ParallelOffsetShape(const TArray<FPolyline>& Polylines, double Offset, const FPCGExCCOffsetOptions& Options)
	{
		return ParallelOffsetShape(Polylines, Offset, FShapeOffsetOptions(Options));
	}
}
