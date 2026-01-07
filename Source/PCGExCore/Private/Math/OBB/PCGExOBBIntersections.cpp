// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Math/OBB/PCGExOBBIntersections.h"
#include "Algo/Unique.h"

namespace PCGExMath::OBB
{
	void FIntersections::Reset(const FVector& InStart, const FVector& InEnd)
	{
		Cuts.Reset();
		Start = InStart;
		End = InEnd;
	}

	void FIntersections::Sort()
	{
		const FVector S = Start;
		Cuts.Sort([&S](const FCut& A, const FCut& B)
		{
			const double DistToA = FVector::DistSquared(S, A.Position);
			const double DistToB = FVector::DistSquared(S, B.Position);
			if (DistToA == DistToB) { return A.Idx < B.Idx; }
			return DistToA < DistToB;
		});
	}

	void FIntersections::SortAndDedupe(float Tolerance)
	{
		Sort();
		Cuts.SetNum(Algo::Unique(Cuts, [](const FCut& A, const FCut& B) { return A.Position == B.Position; }));
	}

	bool SegmentBoxRaw(
		const FOBB& Box, const FVector& Start, const FVector& End,
		FVector& OutHit1, FVector& OutHit2,
		FVector& OutNormal1, FVector& OutNormal2,
		bool& bOutHit2Valid, bool& bOutInverseDir)
	{
		const FMatrix Matrix = Box.GetMatrix();
		const FBox LocalBox = Box.GetLocalBox();

		const FVector LocalStart = Matrix.InverseTransformPosition(Start);
		const FVector LocalEnd = Matrix.InverseTransformPosition(End);

		const bool bStartInside = LocalBox.IsInside(LocalStart);
		const bool bEndInside = LocalBox.IsInside(LocalEnd);

		bOutHit2Valid = false;
		bOutInverseDir = false;

		// Both inside - no surface intersection
		if (bStartInside && bEndInside) return false;

		FVector HitLoc, HitNorm;
		float HitTime;
		bool bHasHit = false;

		// End inside - trace from start to find entry
		if (bEndInside)
		{
			if (FMath::LineExtentBoxIntersection(LocalBox, LocalStart, LocalEnd, FVector::ZeroVector, HitLoc, HitNorm, HitTime))
			{
				OutHit1 = Matrix.TransformPosition(HitLoc);
				OutNormal1 = Matrix.TransformVector(HitNorm).GetSafeNormal();
				return !OutHit1.Equals(Start) && !OutHit1.Equals(End);
			}
			return false;
		}

		// Start inside - trace from end to find exit
		if (bStartInside)
		{
			if (FMath::LineExtentBoxIntersection(LocalBox, LocalEnd, LocalStart, FVector::ZeroVector, HitLoc, HitNorm, HitTime))
			{
				OutHit1 = Matrix.TransformPosition(HitLoc);
				OutNormal1 = Matrix.TransformVector(HitNorm).GetSafeNormal();
				bOutInverseDir = true;
				return !OutHit1.Equals(Start) && !OutHit1.Equals(End);
			}
			return false;
		}

		// Neither inside - check for pass-through
		if (FMath::LineExtentBoxIntersection(LocalBox, LocalStart, LocalEnd, FVector::ZeroVector, HitLoc, HitNorm, HitTime))
		{
			OutHit1 = Matrix.TransformPosition(HitLoc);
			OutNormal1 = Matrix.TransformVector(HitNorm).GetSafeNormal();
			bHasHit = !OutHit1.Equals(Start) && !OutHit1.Equals(End);
		}

		// Check exit
		if (FMath::LineExtentBoxIntersection(LocalBox, LocalEnd, LocalStart, FVector::ZeroVector, HitLoc, HitNorm, HitTime))
		{
			if (!bHasHit)
			{
				OutHit1 = Matrix.TransformPosition(HitLoc);
				OutNormal1 = Matrix.TransformVector(HitNorm).GetSafeNormal();
				bOutInverseDir = true;
				bHasHit = !OutHit1.Equals(Start) && !OutHit1.Equals(End);
			}
			else
			{
				OutHit2 = Matrix.TransformPosition(HitLoc);
				OutNormal2 = Matrix.TransformVector(HitNorm).GetSafeNormal();
				bOutHit2Valid = !OutHit1.Equals(OutHit2) && !OutHit2.Equals(Start) && !OutHit2.Equals(End);
			}
			bHasHit = bHasHit || bOutHit2Valid;
		}

		return bHasHit;
	}

	bool ProcessSegment(const FOBB& Box, FIntersections& IO, int32 CloudIndex)
	{
		FVector Hit1, Hit2, Normal1, Normal2;
		bool bHit2Valid, bInverse;

		if (!SegmentBoxRaw(Box, IO.Start, IO.End, Hit1, Hit2, Normal1, Normal2, bHit2Valid, bInverse))
		{
			return false;
		}

		const int32 BoxIdx = Box.Bounds.Index;

		if (bInverse)
		{
			if (bHit2Valid)
			{
				IO.Add(Hit1, Normal1, BoxIdx, CloudIndex, EPCGExCutType::Exit);
				IO.Add(Hit2, Normal2, BoxIdx, CloudIndex, EPCGExCutType::Entry);
			}
			else
			{
				IO.Add(Hit1, Normal1, BoxIdx, CloudIndex, EPCGExCutType::ExitNoEntry);
			}
		}
		else
		{
			if (bHit2Valid)
			{
				IO.Add(Hit1, Normal1, BoxIdx, CloudIndex, EPCGExCutType::Entry);
				IO.Add(Hit2, Normal2, BoxIdx, CloudIndex, EPCGExCutType::Exit);
			}
			else
			{
				IO.Add(Hit1, Normal1, BoxIdx, CloudIndex, EPCGExCutType::EntryNoExit);
			}
		}

		return true;
	}

	bool SegmentIntersects(const FOBB& Box, const FVector& Start, const FVector& End)
	{
		const FMatrix Matrix = Box.GetMatrix();
		const FBox LocalBox = Box.GetLocalBox();

		const FVector LocalStart = Matrix.InverseTransformPosition(Start);
		const FVector LocalEnd = Matrix.InverseTransformPosition(End);

		if (LocalBox.IsInside(LocalStart) || LocalBox.IsInside(LocalEnd)) { return true; }

		FVector HitLoc, HitNorm;
		float HitTime;
		return FMath::LineExtentBoxIntersection(LocalBox, LocalStart, LocalEnd, FVector::ZeroVector, HitLoc, HitNorm, HitTime);
	}
}
