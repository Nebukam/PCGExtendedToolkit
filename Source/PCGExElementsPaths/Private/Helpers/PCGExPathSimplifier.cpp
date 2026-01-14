// Copyright (c) Nebukam

#include "Helpers/PCGExPathSimplifier.h"

namespace PCGExPaths
{
	FSimplifiedPoint::FSimplifiedPoint(const FTransform& InTransform, int8 bCanRemove)
		: Transform(InTransform), bIsRemovable(bCanRemove)
	{
		TangentIn = FVector::ZeroVector;
		TangentOut = FVector::ZeroVector;
	}

	TArray<FSimplifiedPoint> FCurveSimplifier::SimplifyPolyline(
		const TConstPCGValueRange<FTransform>& InPoints,
		const TArray<int8>& InRemovableFlags,
		float MaxError,
		bool bIsClosed)
	{
		if (InPoints.Num() < 2 || InPoints.Num() != InRemovableFlags.Num())
			return {};

		TArray<FSimplifiedPoint> Result;

		// Handle edge cases
		if (InPoints.Num() <= 3)
		{
			for (int32 i = 0; i < InPoints.Num(); ++i)
			{
				Result.Add(FSimplifiedPoint(InPoints[i], InRemovableFlags[i]));
				Result.Last().OriginalIndex = i;
			}
			// Simple tangent computation for trivial cases
			FitTangentsLeastSquares(Result, InPoints, bIsClosed);
			return Result;
		}

		// Check if any points can be removed
		bool bHasRemovablePoints = false;
		for (int32 i = 0; i < InRemovableFlags.Num(); ++i)
		{
			if (InRemovableFlags[i])
			{
				bHasRemovablePoints = true;
				break;
			}
		}

		if (!bHasRemovablePoints)
		{
			for (int32 i = 0; i < InPoints.Num(); ++i)
			{
				Result.Add(FSimplifiedPoint(InPoints[i], InRemovableFlags[i]));
				Result.Last().OriginalIndex = i;
			}
			FitTangentsLeastSquares(Result, InPoints, bIsClosed);
			return Result;
		}

		// Step 1: Use Douglas-Peucker to select which points to keep
		TArray<int32> SelectedIndices = SimplifyWithDP(InPoints, InRemovableFlags, MaxError, bIsClosed);

		// Step 2: Build result array
		for (int32 Idx : SelectedIndices)
		{
			Result.Add(FSimplifiedPoint(InPoints[Idx], InRemovableFlags[Idx]));
			Result.Last().OriginalIndex = Idx;
		}

		// Step 3: Fit tangents using least-squares
		FitTangentsLeastSquares(Result, InPoints, bIsClosed);

		return Result;
	}

	TArray<int32> FCurveSimplifier::SimplifyWithDP(
		const TConstPCGValueRange<FTransform>& Points,
		const TArray<int8>& RemovableFlags,
		float MaxError,
		bool bIsClosed)
	{
		TArray<int32> ResultIndices;

		if (Points.Num() < 3)
		{
			for (int32 i = 0; i < Points.Num(); ++i)
				ResultIndices.Add(i);
			return ResultIndices;
		}

		// Always keep first point
		ResultIndices.Add(0);

		// Always keep last point for open paths
		if (!bIsClosed)
		{
			ResultIndices.Add(Points.Num() - 1);
		}

		// Keep all non-removable points
		for (int32 i = 1; i < Points.Num() - 1; ++i)
		{
			if (!RemovableFlags[i])
			{
				ResultIndices.AddUnique(i);
			}
		}

		// Handle last point for closed loops
		if (bIsClosed && !RemovableFlags[Points.Num() - 1])
		{
			ResultIndices.AddUnique(Points.Num() - 1);
		}

		// Recursive simplification
		SimplifyRecursive(Points, RemovableFlags, ResultIndices, 0, Points.Num() - 1, MaxError, bIsClosed);

		// For closed loops, also check the wrap-around segment
		if (bIsClosed)
		{
			// Check segment from last point back to first
			float MaxDistance = 0.0f;
			int32 MaxIndex = -1;

			FVector StartPoint = Points[Points.Num() - 1].GetLocation();
			FVector EndPoint = Points[0].GetLocation();

			// No intermediate points in wrap-around for standard DP
			// The wrap is handled by the closed loop tangent fitting
		}

		// Sort and remove duplicates
		ResultIndices.Sort();
		for (int32 i = ResultIndices.Num() - 1; i > 0; --i)
		{
			if (ResultIndices[i] == ResultIndices[i - 1])
			{
				ResultIndices.RemoveAt(i);
			}
		}

		return ResultIndices;
	}

	void FCurveSimplifier::SimplifyRecursive(
		const TConstPCGValueRange<FTransform>& Points,
		const TArray<int8>& RemovableFlags,
		TArray<int32>& SelectedIndices,
		int32 StartIndex,
		int32 EndIndex,
		float MaxError,
		bool bIsClosed)
	{
		if (EndIndex - StartIndex <= 1)
			return;

		float MaxDistance = 0.0f;
		int32 MaxIndex = -1;

		FVector StartPoint = Points[StartIndex].GetLocation();
		FVector EndPoint = Points[EndIndex].GetLocation();

		for (int32 i = StartIndex + 1; i < EndIndex; ++i)
		{
			if (!RemovableFlags[i])
				continue;

			FVector CurrentPoint = Points[i].GetLocation();
			float Distance = PointToLineDistance(CurrentPoint, StartPoint, EndPoint);

			if (Distance > MaxDistance)
			{
				MaxDistance = Distance;
				MaxIndex = i;
			}
		}

		if (MaxDistance > MaxError && MaxIndex != -1)
		{
			SelectedIndices.AddUnique(MaxIndex);
			SimplifyRecursive(Points, RemovableFlags, SelectedIndices, StartIndex, MaxIndex, MaxError, bIsClosed);
			SimplifyRecursive(Points, RemovableFlags, SelectedIndices, MaxIndex, EndIndex, MaxError, bIsClosed);
		}
	}

	float FCurveSimplifier::PointToLineDistance(const FVector& Point, const FVector& LineStart, const FVector& LineEnd)
	{
		FVector LineDirection = LineEnd - LineStart;
		float LineLength = LineDirection.Size();

		if (FMath::IsNearlyZero(LineLength))
			return (Point - LineStart).Size();

		FVector NormalizedLine = LineDirection.GetSafeNormal();
		FVector VectorToPoint = Point - LineStart;
		float Projection = FVector::DotProduct(VectorToPoint, NormalizedLine);
		Projection = FMath::Clamp(Projection, 0.0f, LineLength);

		FVector ClosestPointOnLine = LineStart + NormalizedLine * Projection;
		return (Point - ClosestPointOnLine).Size();
	}

	void FCurveSimplifier::FitTangentsLeastSquares(
		TArray<FSimplifiedPoint>& SimplifiedPoints,
		const TConstPCGValueRange<FTransform>& OriginalPoints,
		bool bIsClosed)
	{
		const int32 NumSimplified = SimplifiedPoints.Num();
		if (NumSimplified < 2)
			return;

		const int32 NumOriginal = OriginalPoints.Num();
		const int32 NumSegments = bIsClosed ? NumSimplified : (NumSimplified - 1);

		// Process each segment independently first
		for (int32 i = 0; i < NumSegments; ++i)
		{
			const int32 NextIdx = (i + 1) % NumSimplified;

			const int32 OrigStartIdx = SimplifiedPoints[i].OriginalIndex;
			const int32 OrigEndIdx = SimplifiedPoints[NextIdx].OriginalIndex;

			const FVector P0 = SimplifiedPoints[i].Transform.GetLocation();
			const FVector P1 = SimplifiedPoints[NextIdx].Transform.GetLocation();

			// Get intermediate points
			TArray<FVector> IntermediatePoints;
			GetIntermediatePoints(OriginalPoints, OrigStartIdx, OrigEndIdx, NumOriginal, bIsClosed, IntermediatePoints);

			// Compute t values using chord-length parameterization
			TArray<float> TValues;
			ComputeChordLengthParams(P0, P1, IntermediatePoints, TValues);

			// Solve for optimal tangents
			FVector T0, T1;
			FitSegmentTangentsLS(P0, P1, IntermediatePoints, TValues, T0, T1);

			// Store results
			// TangentOut of current point, TangentIn of next point
			SimplifiedPoints[i].TangentOut = T0;
			SimplifiedPoints[NextIdx].TangentIn = T1;
		}

		// For points that now have both TangentIn and TangentOut set,
		// we may want to average them or keep them separate for C1 vs G1 continuity

		// For open paths, first point has no TangentIn from a previous segment
		// and last point has no TangentOut to a next segment
		if (!bIsClosed)
		{
			SimplifiedPoints[0].TangentIn = SimplifiedPoints[0].TangentOut;
			SimplifiedPoints[NumSimplified - 1].TangentOut = SimplifiedPoints[NumSimplified - 1].TangentIn;
		}
	}

	void FCurveSimplifier::GetIntermediatePoints(
		const TConstPCGValueRange<FTransform>& OriginalPoints,
		int32 StartIdx,
		int32 EndIdx,
		int32 TotalPoints,
		bool bIsClosed,
		TArray<FVector>& OutPoints)
	{
		OutPoints.Empty();

		if (bIsClosed && EndIdx <= StartIdx)
		{
			// Wrap around case
			for (int32 i = StartIdx + 1; i < TotalPoints; ++i)
			{
				OutPoints.Add(OriginalPoints[i].GetLocation());
			}
			for (int32 i = 0; i < EndIdx; ++i)
			{
				OutPoints.Add(OriginalPoints[i].GetLocation());
			}
		}
		else
		{
			// Normal case
			for (int32 i = StartIdx + 1; i < EndIdx; ++i)
			{
				OutPoints.Add(OriginalPoints[i].GetLocation());
			}
		}
	}

	void FCurveSimplifier::ComputeChordLengthParams(
		const FVector& P0,
		const FVector& P1,
		const TArray<FVector>& IntermediatePoints,
		TArray<float>& OutTValues)
	{
		OutTValues.Empty();

		if (IntermediatePoints.Num() == 0)
			return;

		// Compute cumulative chord lengths
		TArray<float> CumulativeLengths;
		CumulativeLengths.Reserve(IntermediatePoints.Num() + 2);

		float TotalLength = 0.0f;
		FVector PrevPoint = P0;

		CumulativeLengths.Add(0.0f);

		for (const FVector& Point : IntermediatePoints)
		{
			TotalLength += (Point - PrevPoint).Size();
			CumulativeLengths.Add(TotalLength);
			PrevPoint = Point;
		}

		TotalLength += (P1 - PrevPoint).Size();

		// Convert to t values
		if (TotalLength > SMALL_NUMBER)
		{
			for (int32 i = 0; i < IntermediatePoints.Num(); ++i)
			{
				// CumulativeLengths[i+1] because [0] is P0's length (0)
				OutTValues.Add(CumulativeLengths[i + 1] / TotalLength);
			}
		}
		else
		{
			// Fallback to uniform
			for (int32 i = 0; i < IntermediatePoints.Num(); ++i)
			{
				OutTValues.Add(static_cast<float>(i + 1) / static_cast<float>(IntermediatePoints.Num() + 1));
			}
		}
	}

	void FCurveSimplifier::FitSegmentTangentsLS(
		const FVector& P0Pos,
		const FVector& P1Pos,
		const TArray<FVector>& IntermediatePoints,
		const TArray<float>& TValues,
		FVector& OutT0,
		FVector& OutT1)
	{
		const int32 N = IntermediatePoints.Num();
		const FVector ChordDir = P1Pos - P0Pos;
		const float ChordLength = ChordDir.Size();

		// Fallback for no intermediate points: use chord-based tangents
		if (N == 0 || TValues.Num() != N)
		{
			// Catmull-Rom style: tangent magnitude equals chord length
			FVector Dir = ChordDir.GetSafeNormal();
			if (Dir.IsNearlyZero())
			{
				Dir = FVector::ForwardVector;
			}
			OutT0 = Dir * ChordLength;
			OutT1 = Dir * ChordLength;
			return;
		}

		// For a single intermediate point, we can solve exactly
		// For multiple points, we use least squares

		// The cubic Hermite spline is:
		// S(t) = H00(t)*P0 + H10(t)*T0 + H01(t)*P1 + H11(t)*T1
		//
		// We want: Qi ≈ S(ti) for each intermediate point Qi
		//
		// Rearranging: Qi - H00(ti)*P0 - H01(ti)*P1 = H10(ti)*T0 + H11(ti)*T1
		// Let: Ri = Qi - H00(ti)*P0 - H01(ti)*P1
		//
		// Then for each component (x,y,z): Ri.c = H10(ti)*T0.c + H11(ti)*T1.c
		//
		// Matrix form: A * [T0.c, T1.c]^T = [R1.c, R2.c, ..., RN.c]^T
		// where A[i] = [H10(ti), H11(ti)]
		//
		// Least squares solution: [T0.c, T1.c]^T = (A^T * A)^-1 * A^T * R

		// Build A^T * A (2x2 matrix) and A^T * R (2x1 vector per component)
		// A^T * A = [sum(H10*H10), sum(H10*H11)]
		//           [sum(H10*H11), sum(H11*H11)]

		double AtA_00 = 0.0, AtA_01 = 0.0, AtA_11 = 0.0;
		FVector AtR_0 = FVector::ZeroVector;
		FVector AtR_1 = FVector::ZeroVector;

		for (int32 i = 0; i < N; ++i)
		{
			const float t = TValues[i];
			const float h10 = H10(t);
			const float h11 = H11(t);
			const float h00 = H00(t);
			const float h01 = H01(t);

			// Residual: Ri = Qi - H00*P0 - H01*P1
			const FVector Ri = IntermediatePoints[i] - h00 * P0Pos - h01 * P1Pos;

			AtA_00 += h10 * h10;
			AtA_01 += h10 * h11;
			AtA_11 += h11 * h11;

			AtR_0 += h10 * Ri;
			AtR_1 += h11 * Ri;
		}

		// Solve 2x2 system: [AtA_00, AtA_01] [T0]   [AtR_0]
		//                   [AtA_01, AtA_11] [T1] = [AtR_1]

		const double Det = AtA_00 * AtA_11 - AtA_01 * AtA_01;

		if (FMath::Abs(Det) < SMALL_NUMBER)
		{
			// Singular matrix - fall back to chord-based tangents
			FVector Dir = ChordDir.GetSafeNormal();
			if (Dir.IsNearlyZero())
			{
				Dir = FVector::ForwardVector;
			}
			OutT0 = Dir * ChordLength;
			OutT1 = Dir * ChordLength;
			return;
		}

		const double InvDet = 1.0 / Det;

		// Inverse of 2x2: [a b; c d]^-1 = (1/det) * [d -b; -c a]
		// [AtA_00, AtA_01]^-1 = (1/det) * [AtA_11, -AtA_01]
		// [AtA_01, AtA_11]             [-AtA_01, AtA_00]

		OutT0 = (AtA_11 * AtR_0 - AtA_01 * AtR_1) * InvDet;
		OutT1 = (-AtA_01 * AtR_0 + AtA_00 * AtR_1) * InvDet;

		// Only clamp extreme magnitudes - don't mess with direction!
		// The least-squares solution knows best about direction.

		const float MinMag = ChordLength * 0.05f;
		const float MaxMag = ChordLength * 5.0f;

		float Mag0 = OutT0.Size();
		float Mag1 = OutT1.Size();

		if (Mag0 < SMALL_NUMBER)
		{
			// Degenerate - use chord direction
			OutT0 = ChordDir.GetSafeNormal() * ChordLength;
		}
		else if (Mag0 < MinMag || Mag0 > MaxMag)
		{
			// Preserve direction, clamp magnitude
			OutT0 = OutT0.GetSafeNormal() * FMath::Clamp(Mag0, MinMag, MaxMag);
		}

		if (Mag1 < SMALL_NUMBER)
		{
			OutT1 = ChordDir.GetSafeNormal() * ChordLength;
		}
		else if (Mag1 < MinMag || Mag1 > MaxMag)
		{
			OutT1 = OutT1.GetSafeNormal() * FMath::Clamp(Mag1, MinMag, MaxMag);
		}
	}

	FVector FCurveSimplifier::EvaluateHermite(
		const FVector& P0, const FVector& T0,
		const FVector& P1, const FVector& T1,
		float t)
	{
		return H00(t) * P0 + H10(t) * T0 + H01(t) * P1 + H11(t) * T1;
	}
}
