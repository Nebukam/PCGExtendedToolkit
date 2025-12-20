// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "PCGExMath.h"

class UPCGBasePointData;

template <typename ValueType, typename ViewType>
class TPCGValueRange;

template <typename ElementType>
using TConstPCGValueRange = TPCGValueRange<const ElementType, TConstStridedView<ElementType>>;

namespace PCGExData
{
	struct FProxyPoint;
	struct FConstPoint;

	template <typename T>
	class TBuffer;
}

namespace PCGExMath
{
	PCGEXCORE_API bool IntersectOBB_OBB(const FBox& BoxA, const FTransform& TransformA, const FBox& BoxB, const FTransform& TransformB);
	
	template <EPCGExPointBoundsSource S = EPCGExPointBoundsSource::ScaledBounds, typename PointType>
	FORCEINLINE static FBox GetLocalBounds(const PointType& Point)
	{
		if constexpr (S == EPCGExPointBoundsSource::ScaledBounds)
		{
			const FVector Scale = Point.GetTransform().GetScale3D();
			return FBox(Point.GetBoundsMin() * Scale, Point.GetBoundsMax() * Scale);
		}
		else if constexpr (S == EPCGExPointBoundsSource::Bounds)
		{
			return Point.GetLocalBounds();
		}
		else if constexpr (S == EPCGExPointBoundsSource::DensityBounds)
		{
			return Point.GetLocalDensityBounds();
		}
		else if constexpr (S == EPCGExPointBoundsSource::Center)
		{
			return FBox(FVector(-0.001), FVector(0.001));
		}
		else
		{
			return FBox(FVector::OneVector * -1, FVector::OneVector);
		}
	}

	PCGEXCORE_API FBox GetLocalBounds(const PCGExData::FConstPoint& Point, const EPCGExPointBoundsSource Source);

	PCGEXCORE_API FBox GetLocalBounds(const PCGExData::FProxyPoint& Point, const EPCGExPointBoundsSource Source);

	static void SanitizeBounds(FBox& InBox)
	{
		FVector Size = InBox.GetSize();
		for (int i = 0; i < 3; i++)
		{
			if (FMath::IsNaN(Size[i]) || FMath::IsNearlyZero(Size[i])) { InBox.Min[i] -= UE_SMALL_NUMBER; }
		}
	}

	PCGEXCORE_API FBox GetBounds(const TArrayView<FVector> InPositions);

	PCGEXCORE_API FBox GetBounds(const TConstPCGValueRange<FTransform>& InTransforms);

	PCGEXCORE_API FBox GetBounds(const UPCGBasePointData* InPointData, const EPCGExPointBoundsSource Source);
}
