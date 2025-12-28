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

		if (CCWOffsetLoops.IsEmpty() && CWOffsetLoops.IsEmpty())
		{
			return FShape::Empty();
		}

		// Step 2: Find intersections between offset loops
		TArray<FSlicePointSet> SlicePointSets;
		FindIntersectsBetweenOffsetLoops(CCWOffsetLoops, CWOffsetLoops, Options.PosEqualEps, SlicePointSets);

		// Step 3: Create valid slices from intersection points
		TArray<FDissectedSlice> SlicesData;
		CreateValidSlicesFromIntersects(
			CCWOffsetLoops, CWOffsetLoops,
			SlicePointSets, Offset, Options, SlicesData);

		// Step 4: Stitch slices together
		return StitchSlicesTogether(
			SlicesData, CCWOffsetLoops, CWOffsetLoops,
			Options.PosEqualEps, Options.SliceJoinEps);
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
		OffsetOpts.bHandleSelfIntersects = false; // Shape handles intersects differently

		int32 ParentIdx = 0;

		// Process CCW polylines (positive area / outer boundaries)
		for (int32 i = 0; i < CCWPolylines.Num(); ++i)
		{
			const FIndexedPolyline& IPline = CCWPolylines[i];
			const int32 PathId = i < CCWPathIds.Num() ? CCWPathIds[i] : i;

			TArray<FPolyline> OffsetResults = Offset::ParallelOffset(IPline.Polyline, Offset, OffsetOpts);

			for (FPolyline& OffsetPline : OffsetResults)
			{
				const double Area = OffsetPline.Area();

				// Check if orientation inverted (collapsed narrow input)
				// Skip if inversion happened (CCW became CW while offsetting inward)
				if (Offset > 0.0 && Area < 0.0)
				{
					continue;
				}

				OffsetPline.SetPrimaryPathId(PathId);

				if (Area < 0.0)
				{
					OutCWOffsetLoops.Add(FOffsetLoop(ParentIdx, PathId, MoveTemp(OffsetPline)));
				}
				else
				{
					OutCCWOffsetLoops.Add(FOffsetLoop(ParentIdx, PathId, MoveTemp(OffsetPline)));
				}
			}
			++ParentIdx;
		}

		// Process CW polylines (negative area / holes)
		for (int32 i = 0; i < CWPolylines.Num(); ++i)
		{
			const FIndexedPolyline& IPline = CWPolylines[i];
			const int32 PathId = i < CWPathIds.Num() ? CWPathIds[i] : CCWPolylines.Num() + i;

			TArray<FPolyline> OffsetResults = Offset::ParallelOffset(IPline.Polyline, Offset, OffsetOpts);

			for (FPolyline& OffsetPline : OffsetResults)
			{
				const double Area = OffsetPline.Area();

				// Check if orientation inverted (collapsed narrow input)
				// Skip if inversion happened (CW became CCW while offsetting inward)
				if (Offset < 0.0 && Area > 0.0)
				{
					continue;
				}

				OffsetPline.SetPrimaryPathId(PathId);

				if (Area < 0.0)
				{
					OutCWOffsetLoops.Add(FOffsetLoop(ParentIdx, PathId, MoveTemp(OffsetPline)));
				}
				else
				{
					OutCCWOffsetLoops.Add(FOffsetLoop(ParentIdx, PathId, MoveTemp(OffsetPline)));
				}
			}
			++ParentIdx;
		}
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

			for (int32 j = i + 1; j < TotalLoops; ++j)
			{
				const uint64 PairKey = MakePairKey(i, j);
				if (VisitedPairs.Contains(PairKey)) continue;
				VisitedPairs.Add(PairKey);

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

				// Store intersection data
				FSlicePointSet& Set = OutSlicePointSets.AddDefaulted_GetRef();
				Set.LoopIdx1 = i;
				Set.LoopIdx2 = j;
				Set.SlicePoints.Reserve(IntersectionBuffer.Num());

				for (int32 k = 0; k < IntersectionBuffer.Num(); ++k)
				{
					Set.SlicePoints.Add(IntersectionBuffer[k]);
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

		// Build lookup: loop index -> list of slice point set indices
		TMap<int32, TArray<int32>> SlicePointsLookup;
		for (int32 SetIdx = 0; SetIdx < SlicePointSets.Num(); ++SetIdx)
		{
			const FSlicePointSet& Set = SlicePointSets[SetIdx];
			SlicePointsLookup.FindOrAdd(Set.LoopIdx1).Add(SetIdx);
			SlicePointsLookup.FindOrAdd(Set.LoopIdx2).Add(SetIdx);
		}

		// Dissection point structure
		struct FDissectionPoint
		{
			int32 SegIdx;
			FVector2D Pos;
		};

		TArray<FDissectionPoint> SortedIntrs;

		for (int32 LoopIdx = 0; LoopIdx < TotalLoops; ++LoopIdx)
		{
			SortedIntrs.Reset();
			const FOffsetLoop& CurrLoop = GetLoop(LoopIdx, CCWOffsetLoops, CWOffsetLoops);
			const FPolyline& LoopPline = CurrLoop.Polyline;
			const int32 VertCount = LoopPline.VertexCount();

			if (const TArray<int32>* SetIndices = SlicePointsLookup.Find(LoopIdx))
			{
				// Gather all intersects for the current loop
				for (int32 SetIdx : *SetIndices)
				{
					const FSlicePointSet& Set = SlicePointSets[SetIdx];
					const bool bIsFirstIndex = (Set.LoopIdx1 == LoopIdx);

					for (const FBasicIntersect& Intr : Set.SlicePoints)
					{
						FDissectionPoint& DP = SortedIntrs.AddDefaulted_GetRef();
						DP.SegIdx = bIsFirstIndex ? Intr.StartIndex1 : Intr.StartIndex2;
						DP.Pos = Intr.Point;
					}
				}

				// Sort intersect points along direction of polyline
				SortedIntrs.Sort([&LoopPline](const FDissectionPoint& A, const FDissectionPoint& B)
				{
					if (A.SegIdx != B.SegIdx) return A.SegIdx < B.SegIdx;

					const FVector2D SegStart = LoopPline.GetVertex(A.SegIdx).GetPosition();
					const double DistA = Math::DistanceSquared(A.Pos, SegStart);
					const double DistB = Math::DistanceSquared(B.Pos, SegStart);
					return DistA < DistB;
				});

				// Create slices from sorted intersection points
				if (SortedIntrs.Num() <= 1)
				{
					// Treat whole loop as a slice
					FDissectedSlice& Slice = OutSlicesData.AddDefaulted_GetRef();
					Slice.SourceIdx = LoopIdx;
					Slice.StartIndex = 0;
					Slice.EndIndexOffset = VertCount;
					Slice.UpdatedStart = LoopPline.GetVertex(0);
					Slice.UpdatedEndBulge = LoopPline.GetVertex(VertCount - 1).Bulge;
					Slice.EndPoint = LoopPline.GetVertex(0).GetPosition();

					// Validate
					if (!IsSliceValid(Slice, CurrLoop, Offset, Options))
					{
						OutSlicesData.Pop(EAllowShrinking::No);
					}
				}
				else
				{
					// Create slices from adjacent points
					for (int32 i = 0; i < SortedIntrs.Num(); ++i)
					{
						const FDissectionPoint& Pt1 = SortedIntrs[i];
						const FDissectionPoint& Pt2 = SortedIntrs[(i + 1) % SortedIntrs.Num()];

						FDissectedSlice Slice;
						Slice.SourceIdx = LoopIdx;
						Slice.StartIndex = Pt1.SegIdx;
						Slice.EndPoint = Pt2.Pos;

						// Compute end index offset
						if (Pt1.SegIdx == Pt2.SegIdx)
						{
							const FVector2D SegStart = LoopPline.GetVertex(Pt1.SegIdx).GetPosition();
							const double StartDist = Math::DistanceSquared(SegStart, Pt1.Pos);
							const double EndDist = Math::DistanceSquared(SegStart, Pt2.Pos);

							if (EndDist > StartDist + Options.PosEqualEps * Options.PosEqualEps)
							{
								Slice.EndIndexOffset = 0;
							}
							else
							{
								Slice.EndIndexOffset = VertCount;
							}
						}
						else
						{
							int32 EndOffset = Pt2.SegIdx - Pt1.SegIdx;
							if (EndOffset < 0) EndOffset += VertCount;
							Slice.EndIndexOffset = EndOffset;
						}

						// Update start vertex
						const FVertex& OrigStart = LoopPline.GetVertex(Pt1.SegIdx);
						Slice.UpdatedStart = FVertex(Pt1.Pos, OrigStart.Bulge, OrigStart.Source);

						// Update end bulge
						const int32 EndSegIdx = (Pt1.SegIdx + Slice.EndIndexOffset) % VertCount;
						Slice.UpdatedEndBulge = LoopPline.GetVertex(EndSegIdx).Bulge;

						// Validate slice
						if (IsSliceValid(Slice, CurrLoop, Offset, Options))
						{
							OutSlicesData.Add(MoveTemp(Slice));
						}
					}
				}
			}
			else
			{
				// No intersects - test if the whole loop is valid
				FDissectedSlice Slice;
				Slice.SourceIdx = LoopIdx;
				Slice.StartIndex = 0;
				Slice.EndIndexOffset = VertCount;
				Slice.UpdatedStart = LoopPline.GetVertex(0);
				Slice.UpdatedEndBulge = LoopPline.GetVertex(VertCount - 1).Bulge;
				Slice.EndPoint = LoopPline.GetVertex(0).GetPosition();

				if (IsSliceValid(Slice, CurrLoop, Offset, Options))
				{
					OutSlicesData.Add(MoveTemp(Slice));
				}
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

		// Compute midpoint of slice to test
		FVector2D MidPt;
		if (Slice.EndIndexOffset > 1)
		{
			const int32 MidIdx = (Slice.StartIndex + Slice.EndIndexOffset / 2) % VertCount;
			MidPt = LoopPline.GetVertex(MidIdx).GetPosition();
		}
		else
		{
			MidPt = (Slice.GetStartPoint() + Slice.EndPoint) * 0.5;
		}

		const int32 ParentIdx = OffsetLoop.ParentLoopIdx;
		const double AbsOffset = FMath::Abs(Offset);

		// Check against all input polylines (skipping parent)
		for (int32 i = 0; i < CCWPolylines.Num(); ++i)
		{
			if (i == ParentIdx) continue;

			if (!Offset::Internal::PointValidForOffset(
				CCWPolylines[i].Polyline,
				CCWPolylines[i].SpatialIndex,
				AbsOffset, MidPt,
				Options.PosEqualEps, Options.OffsetDistEps))
			{
				return false;
			}
		}

		for (int32 i = 0; i < CWPolylines.Num(); ++i)
		{
			const int32 AdjustedIdx = CCWPolylines.Num() + i;
			if (AdjustedIdx == ParentIdx) continue;

			if (!Offset::Internal::PointValidForOffset(
				CWPolylines[i].Polyline,
				CWPolylines[i].SpatialIndex,
				AbsOffset, MidPt,
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
		const int32 MaxLoopCount = SlicesData.Num();

		for (int32 SliceIdx = 0; SliceIdx < SlicesData.Num(); ++SliceIdx)
		{
			if (VisitedSlices[SliceIdx]) continue;
			VisitedSlices[SliceIdx] = true;

			int32 CurrentIndex = SliceIdx;
			int32 LoopCount = 0;
			FPolyline CurrentPline(false);

			while (LoopCount < MaxLoopCount)
			{
				++LoopCount;

				const FDissectedSlice& CurrSlice = SlicesData[CurrentIndex];
				const FOffsetLoop& SourceLoop = GetLoop(CurrSlice.SourceIdx, CCWOffsetLoops, CWOffsetLoops);
				const FPolyline& SourcePline = SourceLoop.Polyline;
				const int32 N = SourcePline.VertexCount();

				// Add vertices from this slice
				CurrentPline.AddOrReplaceVertex(CurrSlice.UpdatedStart, PosEqualEps);
				CurrentPline.AddContributingPath(SourceLoop.ParentPathId);

				for (int32 j = 1; j <= CurrSlice.EndIndexOffset; ++j)
				{
					const int32 Idx = (CurrSlice.StartIndex + j) % N;
					CurrentPline.AddOrReplaceVertex(SourcePline.GetVertex(Idx), PosEqualEps);
				}

				// Find connecting slice
				const FVector2D EndPoint = CurrSlice.EndPoint;
				int32 ConnectedSliceIdx = INDEX_NONE;
				double MinDistSq = JoinEpsSq * 4.0;

				for (int32 j = 0; j < SlicesData.Num(); ++j)
				{
					if (j == CurrentIndex || VisitedSlices[j]) continue;

					const double DistSq = Math::DistanceSquared(EndPoint, SlicesData[j].GetStartPoint());
					if (DistSq < MinDistSq)
					{
						// Prefer slices from the same source
						if (SlicesData[j].SourceIdx == CurrSlice.SourceIdx || DistSq < JoinEpsSq)
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
						if (CurrentPline.GetVertex(0).PositionEquals(CurrentPline.LastVertex(), PosEqualEps))
						{
							CurrentPline.RemoveLastVertex();
						}
						CurrentPline.SetClosed(true);

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

				// Remove duplicate end vertex before continuing
				CurrentPline.RemoveLastVertex();
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
