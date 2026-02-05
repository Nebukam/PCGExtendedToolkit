// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

/**
 * PCGEx Test Helpers
 *
 * Utility functions and macros for PCGEx automation tests.
 */
namespace PCGExTest
{
	/**
	 * Helper to compare floating point values with tolerance
	 */
	FORCEINLINE bool NearlyEqual(double A, double B, double Tolerance = KINDA_SMALL_NUMBER)
	{
		return FMath::Abs(A - B) <= Tolerance;
	}

	/**
	 * Helper to compare vectors with tolerance
	 */
	FORCEINLINE bool NearlyEqual(const FVector& A, const FVector& B, double Tolerance = KINDA_SMALL_NUMBER)
	{
		return A.Equals(B, Tolerance);
	}

	/**
	 * Helper to compare rotators with tolerance
	 */
	FORCEINLINE bool NearlyEqual(const FRotator& A, const FRotator& B, double Tolerance = KINDA_SMALL_NUMBER)
	{
		return A.Equals(B, Tolerance);
	}

	/**
	 * Helper to compare quaternions with tolerance
	 */
	FORCEINLINE bool NearlyEqual(const FQuat& A, const FQuat& B, double Tolerance = KINDA_SMALL_NUMBER)
	{
		return A.Equals(B, Tolerance);
	}

	/**
	 * Generate a random seed for reproducible tests
	 * Use a fixed base to ensure reproducibility across runs
	 */
	FORCEINLINE uint32 GetTestSeed(int32 TestIndex = 0)
	{
		return 12345u + static_cast<uint32>(TestIndex);
	}

	/**
	 * Generate an array of random positions within bounds
	 */
	PCGEXTENDEDTOOLKITTEST_API TArray<FVector> GenerateRandomPositions(
		int32 NumPoints,
		const FBox& Bounds,
		uint32 Seed);

	/**
	 * Generate a grid of positions
	 */
	PCGEXTENDEDTOOLKITTEST_API TArray<FVector> GenerateGridPositions(
		const FVector& Origin,
		const FVector& Spacing,
		int32 CountX,
		int32 CountY,
		int32 CountZ = 1);

	/**
	 * Generate positions on a sphere surface
	 */
	PCGEXTENDEDTOOLKITTEST_API TArray<FVector> GenerateSpherePositions(
		const FVector& Center,
		double Radius,
		int32 NumPoints,
		uint32 Seed);
}

/**
 * Custom test macros for PCGEx tests
 * These provide better error messages and consistent test patterns
 */

// Test that a value is within tolerance of expected
#define PCGEX_TEST_NEARLY_EQUAL(Actual, Expected, Tolerance, Description) \
	TestTrue(FString::Printf(TEXT("%s: Expected %f, Got %f (Tolerance: %f)"), \
		TEXT(Description), (double)(Expected), (double)(Actual), (double)(Tolerance)), \
		PCGExTest::NearlyEqual((Actual), (Expected), (Tolerance)))

// Test that two vectors are nearly equal
#define PCGEX_TEST_VECTOR_NEARLY_EQUAL(Actual, Expected, Tolerance, Description) \
	TestTrue(FString::Printf(TEXT("%s: Expected (%f, %f, %f), Got (%f, %f, %f)"), \
		TEXT(Description), \
		(Expected).X, (Expected).Y, (Expected).Z, \
		(Actual).X, (Actual).Y, (Actual).Z), \
		PCGExTest::NearlyEqual((Actual), (Expected), (Tolerance)))

// Test that an index is within valid range
#define PCGEX_TEST_VALID_INDEX(Index, Min, Max, Description) \
	TestTrue(FString::Printf(TEXT("%s: Index %d should be in range [%d, %d]"), \
		TEXT(Description), (Index), (Min), (Max)), \
		(Index) >= (Min) && (Index) <= (Max))
