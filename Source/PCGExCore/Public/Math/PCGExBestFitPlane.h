// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include <functional>
#include "CoreMinimal.h"
#include "OrientedBoxTypes.h"

template <typename ValueType, typename ViewType>
class TPCGValueRange;

template <typename ElementType>
using TConstPCGValueRange = TPCGValueRange<const ElementType, TConstStridedView<ElementType>>;

enum class EPCGExAxisOrder : uint8;

namespace PCGExMath
{
	struct PCGEXCORE_API FBestFitPlane
	{
		FBestFitPlane() = default;

		explicit FBestFitPlane(const TConstPCGValueRange<FTransform>& InTransforms);
		explicit FBestFitPlane(const TConstPCGValueRange<FTransform>& InTransforms, TArrayView<int32> InIndices);
		explicit FBestFitPlane(const TArrayView<const FVector> InPositions);
		explicit FBestFitPlane(const TArrayView<const FVector2D> InPositions);

		using FGetElementPositionCallback = std::function<FVector(int32)>;
		FBestFitPlane(const int32 NumElements, FGetElementPositionCallback&& GetPointFunc);
		FBestFitPlane(const int32 NumElements, FGetElementPositionCallback&& GetPointFunc, const FVector& Extra);

		FVector Centroid = FVector::ZeroVector;
		FVector Extents = FVector::OneVector;

		int32 Swizzle[3] = {0, 1, 2};
		FVector Axis[3] = {FVector::ForwardVector, FVector::RightVector, FVector::UpVector};

		FORCEINLINE FVector Normal() const { return Axis[2]; }
		FTransform GetTransform() const;
		FTransform GetTransform(EPCGExAxisOrder Order) const;

	protected:
		void ProcessBox(const UE::Geometry::FOrientedBox3d& Box);
	};

	struct PCGEXCORE_API FSwizzler
	{
		FSwizzler() = default;
	};
}
