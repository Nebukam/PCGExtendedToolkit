// Copyright (c) Nebukam

#pragma once

#include "CoreMinimal.h"
#include "Utils/PCGValueRange.h"

namespace PCGExPaths
{
	struct FSimplifiedPoint
	{
		FTransform Transform = FTransform::Identity;
		FVector TangentIn = FVector::ZeroVector;  // Incoming tangent
		FVector TangentOut = FVector::ZeroVector; // Outgoing tangent
		int8 bIsRemovable = false;
		int32 OriginalIndex = -1;

		FSimplifiedPoint() = default;
		FSimplifiedPoint(const FTransform& InTransform, int8 bCanRemove = true);
	};

	class FCurveSimplifier
	{
	public:
		/**
		 * Simplify a polyline while computing tangents that preserve the original shape.
		 * Uses least-squares fitting to find optimal tangents for cubic Hermite splines.
		 */
		static TArray<FSimplifiedPoint> SimplifyPolyline(
			const TConstPCGValueRange<FTransform>& InPoints,
			const TArray<int8>& InRemovableFlags,
			float MaxError = 0.1f,
			bool bIsClosed = false);

	private:
		// Douglas-Peucker simplification
		static TArray<int32> SimplifyWithDP(
			const TConstPCGValueRange<FTransform>& Points,
			const TArray<int8>& RemovableFlags,
			float MaxError,
			bool bIsClosed);

		static void SimplifyRecursive(
			const TConstPCGValueRange<FTransform>& Points,
			const TArray<int8>& RemovableFlags,
			TArray<int32>& SelectedIndices,
			int32 StartIndex,
			int32 EndIndex,
			float MaxError,
			bool bIsClosed);

		static float PointToLineDistance(const FVector& Point, const FVector& LineStart, const FVector& LineEnd);

		/**
		 * Fit tangents using least-squares optimization.
		 * For each segment, solves for T0 and T1 that minimize error to intermediate points.
		 */
		static void FitTangentsLeastSquares(
			TArray<FSimplifiedPoint>& SimplifiedPoints,
			const TConstPCGValueRange<FTransform>& OriginalPoints,
			bool bIsClosed);

		/**
		 * Solve least-squares for a single segment.
		 * Returns optimal TangentOut for P0 and TangentIn for P1.
		 */
		static void FitSegmentTangentsLS(
			const FVector& P0Pos,
			const FVector& P1Pos,
			const TArray<FVector>& IntermediatePoints,
			const TArray<float>& TValues,
			FVector& OutT0,
			FVector& OutT1);

		/**
		 * Compute chord-length parameterization t values for intermediate points.
		 */
		static void ComputeChordLengthParams(
			const FVector& P0,
			const FVector& P1,
			const TArray<FVector>& IntermediatePoints,
			TArray<float>& OutTValues);

		/**
		 * Get intermediate original points between two simplified points.
		 */
		static void GetIntermediatePoints(
			const TConstPCGValueRange<FTransform>& OriginalPoints,
			int32 StartIdx,
			int32 EndIdx,
			int32 TotalPoints,
			bool bIsClosed,
			TArray<FVector>& OutPoints);

		// Hermite basis functions
		FORCEINLINE static float H00(float t) { return 2.0f * t * t * t - 3.0f * t * t + 1.0f; }
		FORCEINLINE static float H10(float t) { return t * t * t - 2.0f * t * t + t; }
		FORCEINLINE static float H01(float t) { return -2.0f * t * t * t + 3.0f * t * t; }
		FORCEINLINE static float H11(float t) { return t * t * t - t * t; }

		static FVector EvaluateHermite(const FVector& P0, const FVector& T0,
		                               const FVector& P1, const FVector& T1, float t);
	};
}
