// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGData.h"
#include "PCGContext.h"

class UPCGBasePointData;
class UPCGBasePointData;

namespace PCGExTest
{
	/**
	 * Builder pattern for creating test point data
	 *
	 * Example Usage:
	 * @code
	 * UPCGBasePointData* Data = FPointDataBuilder()
	 *     .WithGridPositions(FVector::ZeroVector, FVector(100), 10, 10, 1)
	 *     .WithAttribute<float>(TEXT("Density"), {1.0f, 0.5f, 0.25f})
	 *     .Build();
	 * @endcode
	 */
	class PCGEXTENDEDTOOLKITTEST_API FPointDataBuilder
	{
	public:
		FPointDataBuilder();
		~FPointDataBuilder();

		/**
		 * Set positions using a grid pattern
		 * @param Origin Starting corner of the grid
		 * @param Spacing Distance between points in each axis
		 * @param CountX Number of points in X direction
		 * @param CountY Number of points in Y direction
		 * @param CountZ Number of points in Z direction (default 1 for 2D grid)
		 */
		FPointDataBuilder& WithGridPositions(
			const FVector& Origin,
			const FVector& Spacing,
			int32 CountX,
			int32 CountY,
			int32 CountZ = 1);

		/**
		 * Set positions using random distribution
		 * @param Bounds Bounding box for random positions
		 * @param NumPoints Number of points to generate
		 * @param Seed Random seed for reproducibility
		 */
		FPointDataBuilder& WithRandomPositions(
			const FBox& Bounds,
			int32 NumPoints,
			uint32 Seed = 12345u);

		/**
		 * Set positions from an array
		 * @param Positions Array of positions to use
		 */
		FPointDataBuilder& WithPositions(const TArray<FVector>& Positions);

		/**
		 * Set positions on a sphere surface
		 * @param Center Center of the sphere
		 * @param Radius Radius of the sphere
		 * @param NumPoints Number of points to distribute
		 * @param Seed Random seed for reproducibility
		 */
		FPointDataBuilder& WithSpherePositions(
			const FVector& Center,
			double Radius,
			int32 NumPoints,
			uint32 Seed = 12345u);

		/**
		 * Add a custom attribute to all points
		 * Values are applied cyclically if fewer values than points
		 * @param Name Attribute name
		 * @param Values Array of values to apply
		 */
		template <typename T>
		FPointDataBuilder& WithAttribute(FName Name, const TArray<T>& Values);

		/**
		 * Set uniform scale for all points
		 */
		FPointDataBuilder& WithScale(const FVector& Scale);

		/**
		 * Set uniform rotation for all points
		 */
		FPointDataBuilder& WithRotation(const FRotator& Rotation);

		/**
		 * Build the point data object
		 * @return New UPCGBasePointData with configured points, or nullptr on failure
		 */
		UPCGBasePointData* Build();

	private:
		TArray<FVector> Positions;
		TArray<FRotator> Rotations;
		TArray<FVector> Scales;

		struct FPendingAttribute
		{
			FName Name;
			TFunction<void(UPCGBasePointData*, int32)> ApplyFunc;
		};
		TArray<FPendingAttribute> PendingAttributes;
	};

	/**
	 * Utility functions for verifying point data
	 */
	namespace PointDataVerify
	{
		/**
		 * Verify point count matches expected
		 */
		PCGEXTENDEDTOOLKITTEST_API bool HasPointCount(const UPCGBasePointData* Data, int32 ExpectedCount);

		/**
		 * Verify an attribute exists with expected type
		 */
		template <typename T>
		bool HasAttribute(const UPCGBasePointData* Data, FName AttributeName);

		/**
		 * Get attribute value at index (returns default if not found)
		 */
		template <typename T>
		T GetAttributeValue(const UPCGBasePointData* Data, FName AttributeName, int32 Index, T DefaultValue = T{});
	}
}
