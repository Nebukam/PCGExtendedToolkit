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

	// Public overload: uniform smoothing
	TArray<FSimplifiedPoint> FCurveSimplifier::SimplifyPolyline(
		const TConstPCGValueRange<FTransform>& InPoints,
		const TArray<int8>& InRemovableFlags,
		double MaxError,
		bool bIsClosed,
		double Smoothing,
		EPCGExTangentSmoothing SmoothingMode)
	{
		// Empty array signals uniform smoothing
		return SimplifyPolylineInternal(
			InPoints, InRemovableFlags,
			TArray<double>(), // Empty = use uniform
			Smoothing,
			MaxError, bIsClosed, SmoothingMode);
	}

	// Public overload: per-point smoothing
	TArray<FSimplifiedPoint> FCurveSimplifier::SimplifyPolyline(
		const TConstPCGValueRange<FTransform>& InPoints,
		const TArray<int8>& InRemovableFlags,
		const TArray<double>& InSmoothingValues,
		double MaxError,
		bool bIsClosed,
		EPCGExTangentSmoothing SmoothingMode)
	{
		return SimplifyPolylineInternal(
			InPoints, InRemovableFlags,
			InSmoothingValues,
			0.0f, // Ignored when per-point array is provided
			MaxError, bIsClosed, SmoothingMode);
	}

	// Internal implementation
	TArray<FSimplifiedPoint> FCurveSimplifier::SimplifyPolylineInternal(
		const TConstPCGValueRange<FTransform>& InPoints,
		const TArray<int8>& InRemovableFlags,
		const TArray<double>& InSmoothingValues,
		double UniformSmoothing,
		double MaxError,
		bool bIsClosed,
		EPCGExTangentSmoothing SmoothingMode)
	{
		if (InPoints.Num() < 2 || InPoints.Num() != InRemovableFlags.Num())
			return {};

		// Validate per-point smoothing array if provided
		const bool bHasPerPointSmoothing = InSmoothingValues.Num() > 0;
		if (bHasPerPointSmoothing && InSmoothingValues.Num() != InPoints.Num())
			return {}; // Size mismatch

		TArray<FSimplifiedPoint> Result;

		// Handle edge cases
		if (InPoints.Num() <= 3)
		{
			for (int32 i = 0; i < InPoints.Num(); ++i)
			{
				Result.Add(FSimplifiedPoint(InPoints[i], InRemovableFlags[i]));
				Result.Last().OriginalIndex = i;
			}
			FitTangentsLeastSquares(Result, InPoints, bIsClosed);
			if (SmoothingMode != EPCGExTangentSmoothing::None)
			{
				SmoothTangentsAtJunctions(Result, bIsClosed, InSmoothingValues, UniformSmoothing, SmoothingMode);
			}
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
			if (SmoothingMode != EPCGExTangentSmoothing::None)
			{
				SmoothTangentsAtJunctions(Result, bIsClosed, InSmoothingValues, UniformSmoothing, SmoothingMode);
			}
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

		// Step 4: Smooth tangents at junctions
		if (SmoothingMode != EPCGExTangentSmoothing::None)
		{
			SmoothTangentsAtJunctions(Result, bIsClosed, InSmoothingValues, UniformSmoothing, SmoothingMode);
		}

		return Result;
	}

	void FCurveSimplifier::SmoothTangentsAtJunctions(
		TArray<FSimplifiedPoint>& SimplifiedPoints,
		bool bIsClosed,
		const TArray<double>& InSmoothingValues,
		double UniformSmoothing,
		EPCGExTangentSmoothing SmoothingMode)
	{
		const int32 NumPoints = SimplifiedPoints.Num();
		if (NumPoints < 2)
			return;

		const bool bHasPerPointSmoothing = InSmoothingValues.Num() > 0;

		// For open paths: smooth interior points (indices 1 to N-2)
		// For closed paths: smooth all points
		const int32 StartIdx = bIsClosed ? 0 : 1;
		const int32 EndIdx = bIsClosed ? NumPoints : (NumPoints - 1);

		for (int32 i = StartIdx; i < EndIdx; ++i)
		{
			// Get smoothing value for this point
			double Smoothing;
			if (bHasPerPointSmoothing)
			{
				const int32 OrigIdx = SimplifiedPoints[i].OriginalIndex;
				Smoothing = (OrigIdx >= 0 && OrigIdx < InSmoothingValues.Num())
					            ? InSmoothingValues[OrigIdx]
					            : 0.0f;
			}
			else
			{
				Smoothing = UniformSmoothing;
			}

			// Skip if no smoothing requested for this point
			if (Smoothing <= 0.0f)
				continue;

			SmoothPointTangents(
				SimplifiedPoints[i].TangentIn,
				SimplifiedPoints[i].TangentOut,
				Smoothing,
				SmoothingMode);
		}

		// For open paths, ensure endpoints are consistent
		if (!bIsClosed)
		{
			SimplifiedPoints[0].TangentIn = SimplifiedPoints[0].TangentOut;
			SimplifiedPoints[NumPoints - 1].TangentOut = SimplifiedPoints[NumPoints - 1].TangentIn;
		}
	}

	void FCurveSimplifier::SmoothPointTangents(
		FVector& TangentIn,
		FVector& TangentOut,
		double Smoothing,
		EPCGExTangentSmoothing SmoothingMode)
	{
		Smoothing = FMath::Clamp(Smoothing, 0.0f, 1.0f);

		const double MagIn = TangentIn.Size();
		const double MagOut = TangentOut.Size();

		// Skip if either tangent is degenerate
		if (MagIn < SMALL_NUMBER || MagOut < SMALL_NUMBER)
			return;

		const FVector DirIn = TangentIn / MagIn;
		const FVector DirOut = TangentOut / MagOut;

		if (SmoothingMode == EPCGExTangentSmoothing::DirectionOnly)
		{
			// G1 continuity: blend directions, keep magnitudes
			FVector AvgDir = (DirIn + DirOut);
			const double AvgDirLen = AvgDir.Size();

			if (AvgDirLen < SMALL_NUMBER)
			{
				// Directions are opposite - keep original
				return;
			}

			AvgDir /= AvgDirLen;

			// Lerp from original direction toward average direction
			const FVector SmoothedDirIn = FMath::Lerp(DirIn, AvgDir, Smoothing).GetSafeNormal();
			const FVector SmoothedDirOut = FMath::Lerp(DirOut, AvgDir, Smoothing).GetSafeNormal();

			// Apply smoothed directions with original magnitudes
			TangentIn = SmoothedDirIn * MagIn;
			TangentOut = SmoothedDirOut * MagOut;
		}
		else if (SmoothingMode == EPCGExTangentSmoothing::Full)
		{
			// C1 continuity: blend both direction and magnitude
			const FVector AvgTangent = (TangentIn + TangentOut) * 0.5f;

			// Lerp from original toward the averaged tangent
			TangentIn = FMath::Lerp(TangentIn, AvgTangent, Smoothing);
			TangentOut = FMath::Lerp(TangentOut, AvgTangent, Smoothing);
		}
	}

	TArray<int32> FCurveSimplifier::SimplifyWithDP(
		const TConstPCGValueRange<FTransform>& Points,
		const TArray<int8>& RemovableFlags,
		double MaxError,
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
		double MaxError,
		bool bIsClosed)
	{
		if (EndIndex - StartIndex <= 1)
			return;

		double MaxDistance = 0.0f;
		int32 MaxIndex = -1;

		FVector StartPoint = Points[StartIndex].GetLocation();
		FVector EndPoint = Points[EndIndex].GetLocation();

		for (int32 i = StartIndex + 1; i < EndIndex; ++i)
		{
			if (!RemovableFlags[i])
				continue;

			FVector CurrentPoint = Points[i].GetLocation();
			double Distance = PointToLineDistance(CurrentPoint, StartPoint, EndPoint);

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

	double FCurveSimplifier::PointToLineDistance(const FVector& Point, const FVector& LineStart, const FVector& LineEnd)
	{
		FVector LineDirection = LineEnd - LineStart;
		double LineLength = LineDirection.Size();

		if (FMath::IsNearlyZero(LineLength))
			return (Point - LineStart).Size();

		FVector NormalizedLine = LineDirection.GetSafeNormal();
		FVector VectorToPoint = Point - LineStart;
		double Projection = FVector::DotProduct(VectorToPoint, NormalizedLine);
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

		// Process each segment independently
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
			TArray<double> TValues;
			ComputeChordLengthParams(P0, P1, IntermediatePoints, TValues);

			// Solve for optimal tangents
			FVector T0, T1;
			FitSegmentTangentsLS(P0, P1, IntermediatePoints, TValues, T0, T1);

			// Store results
			SimplifiedPoints[i].TangentOut = T0;
			SimplifiedPoints[NextIdx].TangentIn = T1;
		}

		// For open paths, endpoints need special handling
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
		TArray<double>& OutTValues)
	{
		OutTValues.Empty();

		if (IntermediatePoints.Num() == 0)
			return;

		TArray<double> CumulativeLengths;
		CumulativeLengths.Reserve(IntermediatePoints.Num() + 2);

		double TotalLength = 0.0f;
		FVector PrevPoint = P0;

		CumulativeLengths.Add(0.0f);

		for (const FVector& Point : IntermediatePoints)
		{
			TotalLength += (Point - PrevPoint).Size();
			CumulativeLengths.Add(TotalLength);
			PrevPoint = Point;
		}

		TotalLength += (P1 - PrevPoint).Size();

		if (TotalLength > SMALL_NUMBER)
		{
			for (int32 i = 0; i < IntermediatePoints.Num(); ++i)
			{
				OutTValues.Add(CumulativeLengths[i + 1] / TotalLength);
			}
		}
		else
		{
			for (int32 i = 0; i < IntermediatePoints.Num(); ++i)
			{
				OutTValues.Add(static_cast<double>(i + 1) / static_cast<double>(IntermediatePoints.Num() + 1));
			}
		}
	}

	void FCurveSimplifier::FitSegmentTangentsLS(
		const FVector& P0Pos,
		const FVector& P1Pos,
		const TArray<FVector>& IntermediatePoints,
		const TArray<double>& TValues,
		FVector& OutT0,
		FVector& OutT1)
	{
		const int32 N = IntermediatePoints.Num();
		const FVector ChordDir = P1Pos - P0Pos;
		const double ChordLength = ChordDir.Size();

		// Fallback for no intermediate points
		if (N == 0 || TValues.Num() != N)
		{
			FVector Dir = ChordDir.GetSafeNormal();
			if (Dir.IsNearlyZero())
			{
				Dir = FVector::ForwardVector;
			}
			OutT0 = Dir * ChordLength;
			OutT1 = Dir * ChordLength;
			return;
		}

		// Build least-squares system
		double AtA_00 = 0.0, AtA_01 = 0.0, AtA_11 = 0.0;
		FVector AtR_0 = FVector::ZeroVector;
		FVector AtR_1 = FVector::ZeroVector;

		for (int32 i = 0; i < N; ++i)
		{
			const double t = TValues[i];
			const double h10 = H10(t);
			const double h11 = H11(t);
			const double h00 = H00(t);
			const double h01 = H01(t);

			const FVector Ri = IntermediatePoints[i] - h00 * P0Pos - h01 * P1Pos;

			AtA_00 += h10 * h10;
			AtA_01 += h10 * h11;
			AtA_11 += h11 * h11;

			AtR_0 += h10 * Ri;
			AtR_1 += h11 * Ri;
		}

		const double Det = AtA_00 * AtA_11 - AtA_01 * AtA_01;

		if (FMath::Abs(Det) < SMALL_NUMBER)
		{
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

		OutT0 = (AtA_11 * AtR_0 - AtA_01 * AtR_1) * InvDet;
		OutT1 = (-AtA_01 * AtR_0 + AtA_00 * AtR_1) * InvDet;

		// Only clamp extreme magnitudes
		const double MinMag = ChordLength * 0.05f;
		const double MaxMag = ChordLength * 5.0f;

		double Mag0 = OutT0.Size();
		double Mag1 = OutT1.Size();

		if (Mag0 < SMALL_NUMBER)
		{
			OutT0 = ChordDir.GetSafeNormal() * ChordLength;
		}
		else if (Mag0 < MinMag || Mag0 > MaxMag)
		{
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
		double t)
	{
		return H00(t) * P0 + H10(t) * T0 + H01(t) * P1 + H11(t) * T1;
	}
}
