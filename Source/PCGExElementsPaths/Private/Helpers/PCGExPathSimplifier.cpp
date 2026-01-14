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
		double MaxError,
		bool bIsClosed,
		double Smoothing,
		EPCGExTangentSmoothing SmoothingMode)
	{
		return SimplifyPolylineInternal(
			InPoints, InRemovableFlags,
			TArray<double>(),
			Smoothing,
			MaxError, bIsClosed, SmoothingMode);
	}

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
			0.0,
			MaxError, bIsClosed, SmoothingMode);
	}

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

		const bool bHasPerPointSmoothing = InSmoothingValues.Num() > 0;
		if (bHasPerPointSmoothing && InSmoothingValues.Num() != InPoints.Num())
			return {};

		TArray<FSimplifiedPoint> Result;

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
				SmoothAndRefitTangents(Result, InPoints, bIsClosed, InSmoothingValues, UniformSmoothing, SmoothingMode);
			}
			return Result;
		}

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
				SmoothAndRefitTangents(Result, InPoints, bIsClosed, InSmoothingValues, UniformSmoothing, SmoothingMode);
			}
			return Result;
		}

		TArray<int32> SelectedIndices = SimplifyWithDP(InPoints, InRemovableFlags, MaxError, bIsClosed);

		for (int32 Idx : SelectedIndices)
		{
			Result.Add(FSimplifiedPoint(InPoints[Idx], InRemovableFlags[Idx]));
			Result.Last().OriginalIndex = Idx;
		}

		FitTangentsLeastSquares(Result, InPoints, bIsClosed);

		if (SmoothingMode != EPCGExTangentSmoothing::None)
		{
			SmoothAndRefitTangents(Result, InPoints, bIsClosed, InSmoothingValues, UniformSmoothing, SmoothingMode);
		}

		return Result;
	}

	FVector FCurveSimplifier::ComputeSmoothedDirection(
		const FVector& TangentIn,
		const FVector& TangentOut,
		double Smoothing)
	{
		const double MagIn = TangentIn.Size();
		const double MagOut = TangentOut.Size();

		if (MagIn < SMALL_NUMBER && MagOut < SMALL_NUMBER)
			return FVector::ForwardVector;

		if (MagIn < SMALL_NUMBER)
			return TangentOut.GetSafeNormal();

		if (MagOut < SMALL_NUMBER)
			return TangentIn.GetSafeNormal();

		const FVector DirIn = TangentIn / MagIn;
		const FVector DirOut = TangentOut / MagOut;

		// Check angle between directions
		const double DotProduct = FVector::DotProduct(DirIn, DirOut);

		// Reduce smoothing at sharp corners to prevent overshoot
		// DotProduct = 1 means same direction (full smoothing OK)
		// DotProduct = 0 means 90° angle (reduce smoothing significantly)
		// DotProduct = -1 means opposite directions (no smoothing)
		// 
		// Using squared falloff for more aggressive reduction at corners
		const double AngleFactor = FMath::Max(0.0, (DotProduct + 1.0) * 0.5);
		const double EffectiveSmoothing = Smoothing * AngleFactor * AngleFactor;

		// Compute average direction
		FVector AvgDir = DirIn + DirOut;
		const double AvgLen = AvgDir.Size();

		if (AvgLen < SMALL_NUMBER)
		{
			// Opposite directions - use the one with larger magnitude
			return (MagOut >= MagIn) ? DirOut : DirIn;
		}

		AvgDir /= AvgLen;

		// Blend original directions toward average based on effective smoothing
		const FVector BlendedIn = FMath::Lerp(DirIn, AvgDir, EffectiveSmoothing);
		const FVector BlendedOut = FMath::Lerp(DirOut, AvgDir, EffectiveSmoothing);

		// Return the average of the two blended directions
		return ((BlendedIn + BlendedOut) * 0.5).GetSafeNormal();
	}

	void FCurveSimplifier::SmoothAndRefitTangents(
		TArray<FSimplifiedPoint>& SimplifiedPoints,
		const TConstPCGValueRange<FTransform>& OriginalPoints,
		bool bIsClosed,
		const TArray<double>& InSmoothingValues,
		double UniformSmoothing,
		EPCGExTangentSmoothing SmoothingMode)
	{
		const int32 NumPoints = SimplifiedPoints.Num();
		if (NumPoints < 2)
			return;

		const int32 NumOriginal = OriginalPoints.Num();
		const bool bHasPerPointSmoothing = InSmoothingValues.Num() > 0;

		// Step 1: Compute smoothed directions at each point
		// Also compute effective smoothing (accounting for angle)
		TArray<FVector> SmoothedDirections;
		TArray<double> EffectiveSmoothingValues;
		SmoothedDirections.SetNum(NumPoints);
		EffectiveSmoothingValues.SetNum(NumPoints);

		for (int32 i = 0; i < NumPoints; ++i)
		{
			double Smoothing;
			if (bHasPerPointSmoothing)
			{
				const int32 OrigIdx = SimplifiedPoints[i].OriginalIndex;
				Smoothing = (OrigIdx >= 0 && OrigIdx < InSmoothingValues.Num())
					            ? FMath::Clamp(InSmoothingValues[OrigIdx], 0.0, 1.0)
					            : 0.0;
			}
			else
			{
				Smoothing = FMath::Clamp(UniformSmoothing, 0.0, 1.0);
			}

			// For endpoints of open paths, no smoothing needed
			if (!bIsClosed && (i == 0 || i == NumPoints - 1))
			{
				const FVector& Tangent = (i == 0) ? SimplifiedPoints[i].TangentOut : SimplifiedPoints[i].TangentIn;
				SmoothedDirections[i] = Tangent.GetSafeNormal();
				EffectiveSmoothingValues[i] = 0.0;
			}
			else
			{
				// Compute angle-adjusted effective smoothing
				const FVector& TangentIn = SimplifiedPoints[i].TangentIn;
				const FVector& TangentOut = SimplifiedPoints[i].TangentOut;

				const double MagIn = TangentIn.Size();
				const double MagOut = TangentOut.Size();

				if (MagIn > SMALL_NUMBER && MagOut > SMALL_NUMBER)
				{
					const FVector DirIn = TangentIn / MagIn;
					const FVector DirOut = TangentOut / MagOut;
					const double DotProduct = FVector::DotProduct(DirIn, DirOut);

					// Squared falloff for aggressive reduction at sharp angles
					const double AngleFactor = FMath::Max(0.0, (DotProduct + 1.0) * 0.5);
					EffectiveSmoothingValues[i] = Smoothing * AngleFactor * AngleFactor;
				}
				else
				{
					EffectiveSmoothingValues[i] = 0.0;
				}

				SmoothedDirections[i] = ComputeSmoothedDirection(TangentIn, TangentOut, Smoothing);
			}
		}

		// Step 2: Re-fit magnitudes for each segment using the smoothed directions
		const int32 NumSegments = bIsClosed ? NumPoints : (NumPoints - 1);

		TArray<double> MagnitudesOut;
		TArray<double> MagnitudesIn;
		MagnitudesOut.SetNumZeroed(NumPoints);
		MagnitudesIn.SetNumZeroed(NumPoints);

		for (int32 i = 0; i < NumSegments; ++i)
		{
			const int32 NextIdx = (i + 1) % NumPoints;

			const int32 OrigStartIdx = SimplifiedPoints[i].OriginalIndex;
			const int32 OrigEndIdx = SimplifiedPoints[NextIdx].OriginalIndex;

			const FVector P0 = SimplifiedPoints[i].Transform.GetLocation();
			const FVector P1 = SimplifiedPoints[NextIdx].Transform.GetLocation();

			const FVector Dir0 = SmoothedDirections[i];
			const FVector Dir1 = SmoothedDirections[NextIdx];

			TArray<FVector> IntermediatePoints;
			GetIntermediatePoints(OriginalPoints, OrigStartIdx, OrigEndIdx, NumOriginal, bIsClosed, IntermediatePoints);

			TArray<double> TValues;
			ComputeChordLengthParams(P0, P1, IntermediatePoints, TValues);

			double Mag0, Mag1;
			FitSegmentMagnitudes(P0, P1, Dir0, Dir1, IntermediatePoints, TValues, Mag0, Mag1);

			MagnitudesOut[i] = Mag0;
			MagnitudesIn[NextIdx] = Mag1;
		}

		// Step 3: Apply the results using effective smoothing
		for (int32 i = 0; i < NumPoints; ++i)
		{
			const double EffectiveSmoothing = EffectiveSmoothingValues[i];

			if (SmoothingMode == EPCGExTangentSmoothing::Full)
			{
				// C1: Same direction AND magnitude for in/out
				double AvgMag;
				if (!bIsClosed && i == 0)
				{
					AvgMag = MagnitudesOut[i];
				}
				else if (!bIsClosed && i == NumPoints - 1)
				{
					AvgMag = MagnitudesIn[i];
				}
				else
				{
					AvgMag = (MagnitudesIn[i] + MagnitudesOut[i]) * 0.5;
				}

				const FVector SmoothedTangent = SmoothedDirections[i] * AvgMag;

				SimplifiedPoints[i].TangentIn = FMath::Lerp(SimplifiedPoints[i].TangentIn, SmoothedTangent, EffectiveSmoothing);
				SimplifiedPoints[i].TangentOut = FMath::Lerp(SimplifiedPoints[i].TangentOut, SmoothedTangent, EffectiveSmoothing);
			}
			else if (SmoothingMode == EPCGExTangentSmoothing::DirectionOnly)
			{
				// G1: Same direction, different magnitudes
				const FVector SmoothedIn = SmoothedDirections[i] * MagnitudesIn[i];
				const FVector SmoothedOut = SmoothedDirections[i] * MagnitudesOut[i];

				SimplifiedPoints[i].TangentIn = FMath::Lerp(SimplifiedPoints[i].TangentIn, SmoothedIn, EffectiveSmoothing);
				SimplifiedPoints[i].TangentOut = FMath::Lerp(SimplifiedPoints[i].TangentOut, SmoothedOut, EffectiveSmoothing);
			}
		}

		// Ensure open path endpoints are consistent
		if (!bIsClosed)
		{
			SimplifiedPoints[0].TangentIn = SimplifiedPoints[0].TangentOut;
			SimplifiedPoints[NumPoints - 1].TangentOut = SimplifiedPoints[NumPoints - 1].TangentIn;
		}
	}

	void FCurveSimplifier::FitSegmentMagnitudes(
		const FVector& P0Pos,
		const FVector& P1Pos,
		const FVector& Dir0,
		const FVector& Dir1,
		const TArray<FVector>& IntermediatePoints,
		const TArray<double>& TValues,
		double& OutMag0,
		double& OutMag1)
	{
		const FVector ChordDir = P1Pos - P0Pos;
		const double ChordLength = ChordDir.Size();
		const int32 N = IntermediatePoints.Num();

		if (N == 0 || TValues.Num() != N || ChordLength < SMALL_NUMBER)
		{
			OutMag0 = ChordLength;
			OutMag1 = ChordLength;
			return;
		}

		double AtA_00 = 0.0, AtA_01 = 0.0, AtA_11 = 0.0;
		double AtR_0 = 0.0, AtR_1 = 0.0;

		const double D0DotD1 = FVector::DotProduct(Dir0, Dir1);

		for (int32 i = 0; i < N; ++i)
		{
			const double t = TValues[i];
			const double h10 = H10(t);
			const double h11 = H11(t);
			const double h00 = H00(t);
			const double h01 = H01(t);

			const FVector Ri = IntermediatePoints[i] - h00 * P0Pos - h01 * P1Pos;

			AtA_00 += h10 * h10;
			AtA_01 += h10 * h11 * D0DotD1;
			AtA_11 += h11 * h11;

			AtR_0 += h10 * FVector::DotProduct(Dir0, Ri);
			AtR_1 += h11 * FVector::DotProduct(Dir1, Ri);
		}

		const double Det = AtA_00 * AtA_11 - AtA_01 * AtA_01;

		if (FMath::Abs(Det) < SMALL_NUMBER)
		{
			OutMag0 = ChordLength;
			OutMag1 = ChordLength;
			return;
		}

		const double InvDet = 1.0 / Det;

		OutMag0 = (AtA_11 * AtR_0 - AtA_01 * AtR_1) * InvDet;
		OutMag1 = (-AtA_01 * AtR_0 + AtA_00 * AtR_1) * InvDet;

		// Clamp to reasonable range and ensure positive
		const double MinMag = ChordLength * 0.05;
		const double MaxMag = ChordLength * 3.0; // Reduced from 5.0 to prevent overshoot

		OutMag0 = FMath::Clamp(FMath::Abs(OutMag0), MinMag, MaxMag);
		OutMag1 = FMath::Clamp(FMath::Abs(OutMag1), MinMag, MaxMag);
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

		ResultIndices.Add(0);

		if (!bIsClosed)
		{
			ResultIndices.Add(Points.Num() - 1);
		}

		for (int32 i = 1; i < Points.Num() - 1; ++i)
		{
			if (!RemovableFlags[i])
			{
				ResultIndices.AddUnique(i);
			}
		}

		if (bIsClosed && !RemovableFlags[Points.Num() - 1])
		{
			ResultIndices.AddUnique(Points.Num() - 1);
		}

		SimplifyRecursive(Points, RemovableFlags, ResultIndices, 0, Points.Num() - 1, MaxError, bIsClosed);

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

		double MaxDistance = 0.0;
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
		Projection = FMath::Clamp(Projection, 0.0, LineLength);

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

		for (int32 i = 0; i < NumSegments; ++i)
		{
			const int32 NextIdx = (i + 1) % NumSimplified;

			const int32 OrigStartIdx = SimplifiedPoints[i].OriginalIndex;
			const int32 OrigEndIdx = SimplifiedPoints[NextIdx].OriginalIndex;

			const FVector P0 = SimplifiedPoints[i].Transform.GetLocation();
			const FVector P1 = SimplifiedPoints[NextIdx].Transform.GetLocation();

			TArray<FVector> IntermediatePoints;
			GetIntermediatePoints(OriginalPoints, OrigStartIdx, OrigEndIdx, NumOriginal, bIsClosed, IntermediatePoints);

			TArray<double> TValues;
			ComputeChordLengthParams(P0, P1, IntermediatePoints, TValues);

			FVector T0, T1;
			FitSegmentTangentsLS(P0, P1, IntermediatePoints, TValues, T0, T1);

			SimplifiedPoints[i].TangentOut = T0;
			SimplifiedPoints[NextIdx].TangentIn = T1;
		}

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

		double TotalLength = 0.0;
		FVector PrevPoint = P0;

		CumulativeLengths.Add(0.0);

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

		const double MinMag = ChordLength * 0.05;
		const double MaxMag = ChordLength * 3.0; // Reduced from 5.0

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
