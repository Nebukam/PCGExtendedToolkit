// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "PCGExMath.h"

namespace PCGExData
{
	struct FProxyPoint;
	struct FConstPoint;

	template <typename T>
	class TBuffer;
}

namespace PCGExMath
{
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

	PCGEXTENDEDTOOLKIT_API FBox GetLocalBounds(const PCGExData::FConstPoint& Point, const EPCGExPointBoundsSource Source);

	PCGEXTENDEDTOOLKIT_API FBox GetLocalBounds(const PCGExData::FProxyPoint& Point, const EPCGExPointBoundsSource Source);
}
