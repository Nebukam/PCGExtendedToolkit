// Copyright (c) Nebukam

#pragma once

#include "CoreMinimal.h"
#include "Utils/PCGValueRange.h"

#include "PCGExPathSimplifier.generated.h"

UENUM(BlueprintType)
enum class EPCGExTangentSmoothing : uint8
{
	None UMETA(DisplayName = "None (Accurate)", ToolTip="Keep tangents separate - most accurate to original curve"),
	DirectionOnly UMETA(DisplayName = "Direction Only (G1)", ToolTip="Blend directions only, re-optimize magnitudes"),
	Full UMETA(DisplayName = "Full (C1)", ToolTip="Fully matched tangents with optimized magnitudes")
};

namespace PCGExPaths
{
	struct FSimplifiedPoint
	{
		FTransform Transform = FTransform::Identity;
		FVector TangentIn = FVector::ZeroVector;
		FVector TangentOut = FVector::ZeroVector;
		int8 bIsRemovable = false;
		int32 OriginalIndex = -1;

		FSimplifiedPoint() = default;
		FSimplifiedPoint(const FTransform& InTransform, int8 bCanRemove = true);
	};

	class FCurveSimplifier
	{
	public:
		/**
		 * Simplify a polyline with uniform smoothing applied to all points.
		 */
		static TArray<FSimplifiedPoint> SimplifyPolyline(
			const TConstPCGValueRange<FTransform>& InPoints,
			const TArray<int8>& InRemovableFlags,
			double MaxError = 0.1,
			bool bIsClosed = false,
			double Smoothing = 0.0,
			EPCGExTangentSmoothing SmoothingMode = EPCGExTangentSmoothing::Full);

		/**
		 * Simplify a polyline with per-point smoothing control.
		 */
		static TArray<FSimplifiedPoint> SimplifyPolyline(
			const TConstPCGValueRange<FTransform>& InPoints,
			const TArray<int8>& InRemovableFlags,
			const TArray<double>& InSmoothingValues,
			double MaxError = 0.1,
			bool bIsClosed = false,
			EPCGExTangentSmoothing SmoothingMode = EPCGExTangentSmoothing::Full);

	private:
		static TArray<FSimplifiedPoint> SimplifyPolylineInternal(
			const TConstPCGValueRange<FTransform>& InPoints,
			const TArray<int8>& InRemovableFlags,
			const TArray<double>& InSmoothingValues,
			double UniformSmoothing,
			double MaxError,
			bool bIsClosed,
			EPCGExTangentSmoothing SmoothingMode);

		static TArray<int32> SimplifyWithDP(
			const TConstPCGValueRange<FTransform>& Points,
			const TArray<int8>& RemovableFlags,
			double MaxError,
			bool bIsClosed);

		static void SimplifyRecursive(
			const TConstPCGValueRange<FTransform>& Points,
			const TArray<int8>& RemovableFlags,
			TArray<int32>& SelectedIndices,
			int32 StartIndex,
			int32 EndIndex,
			double MaxError,
			bool bIsClosed);

		static double PointToLineDistance(const FVector& Point, const FVector& LineStart, const FVector& LineEnd);

		static void FitTangentsLeastSquares(
			TArray<FSimplifiedPoint>& SimplifiedPoints,
			const TConstPCGValueRange<FTransform>& OriginalPoints,
			bool bIsClosed);

		static void SmoothAndRefitTangents(
			TArray<FSimplifiedPoint>& SimplifiedPoints,
			const TConstPCGValueRange<FTransform>& OriginalPoints,
			bool bIsClosed,
			const TArray<double>& InSmoothingValues,
			double UniformSmoothing,
			EPCGExTangentSmoothing SmoothingMode);

		/**
		 * Compute the effective smoothing at a point, reduced for sharp corners.
		 */
		static double ComputeEffectiveSmoothing(
			const FVector& TangentIn,
			const FVector& TangentOut,
			double RequestedSmoothing);

		/**
		 * Compute smoothed direction, respecting corner sharpness.
		 */
		static FVector ComputeSmoothedDirection(
			const FVector& TangentIn,
			const FVector& TangentOut,
			double Smoothing);

		/**
		 * Re-fit magnitudes for a segment given fixed directions.
		 * Returns false if the fit is invalid (would require negative magnitudes).
		 */
		static void FitSegmentMagnitudes(
			const FVector& P0Pos,
			const FVector& P1Pos,
			const FVector& Dir0,
			const FVector& Dir1,
			const TArray<FVector>& IntermediatePoints,
			const TArray<double>& TValues,
			double& OutMag0,
			double& OutMag1);

		static void FitSegmentTangentsLS(
			const FVector& P0Pos,
			const FVector& P1Pos,
			const TArray<FVector>& IntermediatePoints,
			const TArray<double>& TValues,
			FVector& OutT0,
			FVector& OutT1);

		static void ComputeChordLengthParams(
			const FVector& P0,
			const FVector& P1,
			const TArray<FVector>& IntermediatePoints,
			TArray<double>& OutTValues);

		static void GetIntermediatePoints(
			const TConstPCGValueRange<FTransform>& OriginalPoints,
			int32 StartIdx,
			int32 EndIdx,
			int32 TotalPoints,
			bool bIsClosed,
			TArray<FVector>& OutPoints);

		FORCEINLINE static double H00(double t) { return 2.0 * t * t * t - 3.0 * t * t + 1.0; }
		FORCEINLINE static double H10(double t) { return t * t * t - 2.0 * t * t + t; }
		FORCEINLINE static double H01(double t) { return -2.0 * t * t * t + 3.0 * t * t; }
		FORCEINLINE static double H11(double t) { return t * t * t - t * t; }

		static FVector EvaluateHermite(const FVector& P0, const FVector& T0,
		                               const FVector& P1, const FVector& T1, double t);
	};
}