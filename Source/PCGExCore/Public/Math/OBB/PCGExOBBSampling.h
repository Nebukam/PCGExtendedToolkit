// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOBB.h"

namespace PCGExMath::OBB
{
	// Sample result - what you get from sampling a point against an OBB
	struct PCGEXCORE_API FSample
	{
		FVector Distances = FVector::ZeroVector;  // Local position = signed distances from center
		FVector UVW = FVector::ZeroVector;        // Normalized [-1, 1] position within box
		double Weight = 0.0;                       // Weight based on position (for blending)
		int32 BoxIndex = -1;
		bool bIsInside = false;

		FORCEINLINE FSample(){};
	};

	// Sampling functions

	/**
	 * Sample a world position against an OBB.
	 * Computes local position, UVW coordinates, and weight.
	 * Weight is based on normalized distance from center (1 at center, 0 at edge).
	 */
	FORCEINLINE void Sample(const FOBB& Box, const FVector& WorldPosition, FSample& OutSample)
	{
		const FVector Local = Box.ToLocal(WorldPosition);
		const FVector& E = Box.Bounds.HalfExtents;

		// Inside test
		OutSample.bIsInside =
			FMath::Abs(Local.X) <= E.X &&
			FMath::Abs(Local.Y) <= E.Y &&
			FMath::Abs(Local.Z) <= E.Z;

		OutSample.BoxIndex = Box.Bounds.Index;
		OutSample.Distances = Local;

		// UVW: normalized position [-1, 1]
		OutSample.UVW = FVector(
			E.X > KINDA_SMALL_NUMBER ? Local.X / E.X : 0.0f,
			E.Y > KINDA_SMALL_NUMBER ? Local.Y / E.Y : 0.0f,
			E.Z > KINDA_SMALL_NUMBER ? Local.Z / E.Z : 0.0f
		);

		// Weight: 1 at center, 0 at edge (based on max axis ratio)
		const double MaxRatio = FMath::Max3(
			FMath::Abs(OutSample.UVW.X),
			FMath::Abs(OutSample.UVW.Y),
			FMath::Abs(OutSample.UVW.Z)
		);
		OutSample.Weight = OutSample.bIsInside ? FMath::Clamp(1.0 - MaxRatio, 0.0, 1.0) : 0.0;
	}

	/**
	 * Quick sample - only computes what's needed for containment + distance.
	 * Faster when you don't need UVW or weight.
	 */
	FORCEINLINE void SampleFast(const FOBB& Box, const FVector& WorldPosition, FSample& OutSample)
	{
		const FVector Local = Box.ToLocal(WorldPosition);
		const FVector& E = Box.Bounds.HalfExtents;

		OutSample.bIsInside =
			FMath::Abs(Local.X) <= E.X &&
			FMath::Abs(Local.Y) <= E.Y &&
			FMath::Abs(Local.Z) <= E.Z;

		OutSample.BoxIndex = Box.Bounds.Index;
		OutSample.Distances = Local;
	}

	/**
	 * Sample with custom weight function.
	 * WeightFunc takes UVW and returns weight.
	 */
	template <typename WeightFunc>
	FORCEINLINE void SampleWithWeight(const FOBB& Box, const FVector& WorldPosition, FSample& OutSample, WeightFunc&& ComputeWeight)
	{
		const FVector Local = Box.ToLocal(WorldPosition);
		const FVector& E = Box.Bounds.HalfExtents;

		OutSample.bIsInside =
			FMath::Abs(Local.X) <= E.X &&
			FMath::Abs(Local.Y) <= E.Y &&
			FMath::Abs(Local.Z) <= E.Z;

		OutSample.BoxIndex = Box.Bounds.Index;
		OutSample.Distances = Local;

		OutSample.UVW = FVector(
			E.X > KINDA_SMALL_NUMBER ? Local.X / E.X : 0.0f,
			E.Y > KINDA_SMALL_NUMBER ? Local.Y / E.Y : 0.0f,
			E.Z > KINDA_SMALL_NUMBER ? Local.Z / E.Z : 0.0f
		);

		OutSample.Weight = OutSample.bIsInside ? ComputeWeight(OutSample.UVW) : 0.0;
	}

}