// Copyright (c) Nebukam

#pragma once

#include "CoreMinimal.h"
#include "Utils/PCGValueRange.h"

#include "PCGExPathSimplifier.generated.h"

UENUM(BlueprintType)
enum class EPCGExTangentSmoothing : uint8
{
	None UMETA(DisplayName = "None (Accurate)", ToolTip="Keep tangents separate - most accurate to original curve"),
	DirectionOnly UMETA(DisplayName = "Direction Only (G1)", ToolTip="Blend directions only - smooth joins, preserves speed variation"),
	Full UMETA(DisplayName = "Full (C1)", ToolTip="Fully blend tangents - smoothest joins")
};

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
		 * Simplify a polyline with uniform smoothing applied to all points.
		 * 
		 * @param InPoints          The original point transforms
		 * @param InRemovableFlags  Which points can be removed (true = removable)
		 * @param MaxError          Maximum allowed deviation from original path
		 * @param bIsClosed         Whether this is a closed loop
		 * @param Smoothing         How much to blend tangents at junctions (0 = precise, 1 = smooth)
		 * @param SmoothingMode     Type of smoothing to apply
		 */
		static TArray<FSimplifiedPoint> SimplifyPolyline(
			const TConstPCGValueRange<FTransform>& InPoints,
			const TArray<int8>& InRemovableFlags,
			double MaxError = 0.1f,
			bool bIsClosed = false,
			double Smoothing = 0.0f,
			EPCGExTangentSmoothing SmoothingMode = EPCGExTangentSmoothing::Full);

		/**
		 * Simplify a polyline with per-point smoothing control.
		 * 
		 * @param InPoints              The original point transforms
		 * @param InRemovableFlags      Which points can be removed (true = removable)
		 * @param InSmoothingValues     Per-point smoothing values (0 = precise, 1 = smooth). Must match InPoints size.
		 * @param MaxError              Maximum allowed deviation from original path
		 * @param bIsClosed             Whether this is a closed loop
		 * @param SmoothingMode         Type of smoothing to apply
		 */
		static TArray<FSimplifiedPoint> SimplifyPolyline(
			const TConstPCGValueRange<FTransform>& InPoints,
			const TArray<int8>& InRemovableFlags,
			const TArray<double>& InSmoothingValues,
			double MaxError = 0.1f,
			bool bIsClosed = false,
			EPCGExTangentSmoothing SmoothingMode = EPCGExTangentSmoothing::Full);

	private:
		/**
		 * Core implementation that handles both uniform and per-point smoothing.
		 * @param InSmoothingValues If empty, uses uniform smoothing. Otherwise per-point.
		 * @param UniformSmoothing  Used when InSmoothingValues is empty.
		 */
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

		/**
		 * Smooth tangents at junctions with per-point control.
		 * @param InSmoothingValues Per-point smoothing indexed by OriginalIndex. Empty = no smoothing.
		 * @param UniformSmoothing  Fallback when InSmoothingValues is empty.
		 */
		static void SmoothTangentsAtJunctions(
			TArray<FSimplifiedPoint>& SimplifiedPoints,
			bool bIsClosed,
			const TArray<double>& InSmoothingValues,
			double UniformSmoothing,
			EPCGExTangentSmoothing SmoothingMode);

		/**
		 * Apply smoothing to a single point's tangents.
		 */
		static void SmoothPointTangents(
			FVector& TangentIn,
			FVector& TangentOut,
			double Smoothing,
			EPCGExTangentSmoothing SmoothingMode);

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

		// Hermite basis functions
		FORCEINLINE static double H00(double t) { return 2.0f * t * t * t - 3.0f * t * t + 1.0f; }
		FORCEINLINE static double H10(double t) { return t * t * t - 2.0f * t * t + t; }
		FORCEINLINE static double H01(double t) { return -2.0f * t * t * t + 3.0f * t * t; }
		FORCEINLINE static double H11(double t) { return t * t * t - t * t; }

		static FVector EvaluateHermite(const FVector& P0, const FVector& T0,
		                               const FVector& P1, const FVector& T1, double t);
	};
}
