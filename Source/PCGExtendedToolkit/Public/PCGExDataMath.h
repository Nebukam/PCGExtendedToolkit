// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "PCGExMath.h"
#include "Data/PCGExData.h"

//#include "PCGExDataMath.generated.h"

namespace PCGExMath
{
	PCGEXTENDEDTOOLKIT_API
	FVector NRM(
		const int32 A, const int32 B, const int32 C,
		const TArray<FVector>& InPositions,
		const PCGExData::TBuffer<FVector>* UpVectorCache = nullptr,
		const FVector& UpVector = FVector::UpVector);

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

	PCGEXTENDEDTOOLKIT_API
	FBox GetLocalBounds(const PCGExData::FConstPoint& Point, const EPCGExPointBoundsSource Source);
	
	PCGEXTENDEDTOOLKIT_API
	FBox GetLocalBounds(const PCGExData::FProxyPoint& Point, const EPCGExPointBoundsSource Source);


#pragma region Spatialized distances

	template <EPCGExDistance Mode, typename PointType>
	static FVector GetSpatializedCenter(
		const PointType& FromPoint,
		const FVector& FromCenter,
		const FVector& ToCenter)
	{
		if constexpr (Mode == EPCGExDistance::None)
		{
			return FVector::OneVector;
		}
		else if constexpr (Mode == EPCGExDistance::SphereBounds)
		{
			FVector Dir = ToCenter - FromCenter;
			Dir.Normalize();

			return FromCenter + Dir * FromPoint.GetScaledExtents().Length();
		}
		else if constexpr (Mode == EPCGExDistance::BoxBounds)
		{
			const FVector LocalTargetCenter = FromPoint.GetTransform().InverseTransformPosition(ToCenter);

			const double DistanceSquared = ComputeSquaredDistanceFromBoxToPoint(FromPoint.GetBoundsMin(), FromPoint.GetBoundsMax(), LocalTargetCenter);

			FVector Dir = -LocalTargetCenter;
			Dir.Normalize();

			const FVector LocalClosestPoint = LocalTargetCenter + Dir * FMath::Sqrt(DistanceSquared);

			return FromPoint.GetTransform().TransformPosition(LocalClosestPoint);
		}
		else
		{
			return FromCenter;
		}
	}

#pragma endregion
}
